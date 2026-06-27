/*
 * Trabalho -- Central para Elevador (6 andares)
 * ELE1012 -- Microcontroladores -- UTFPR Medianeira
 * PIC18F4550 | Fosc = 20 MHz | MPLAB v8.92 + C18 v3.47 | PICSimLab
 *
 * DEPURACAO DO TRIMPOT (ADC AN0 / RA0):
 *   1. Confirme DIP switch "ANAL0" (posicao 9, fileira SUPERIOR) = ON
 *      Sem isso o trimpot fica desconectado de RA0 fisicamente.
 *   2. No PICSimLab: gire o trimpot ANAL0 (circulo BRANCO no CENTRO da placa) com scroll do mouse.
*      NAO confundir com o trimpot AZUL ao lado do LCD -- esse e so contraste, sem ligacao ao PIC.
 *      Rolar para cima = tensao maior = temperatura maior.
 *   3. Se o LCD travar em "Tmp: 0.0C" sempre, suspeite que ADC nao converte.
 *      Coloque um breakpoint em `return` dentro de ler_adc() e verifique
 *      ADRESH e ADRESL no Watch Window.
 *   4. Se ADIF nunca sobe, o canal AN0 pode ainda estar configurado como
 *      digital -- verifique ADCON1.PCFG = 0b1110 apos init_adc().
 *
 * -- CONEXOES ----------------------------------------------------
 *  RA0/AN0   potenciometro ANAL0  (simula temperatura 0-50 grausC)
 *  RB0-RB5   botoes andares 1-6  (entrada, pull-up, ativo em LOW)
 *  RC2/CCP1  PWM motor           (~7 kHz, 38% duty cycle)
 *  RD0       motor IN1           (saida -- L293D direcao)
 *  RD1       motor IN2           (saida -- L293D direcao)
 *  RD2       LED porta           (1 = porta aberta, 0 = fechada)
 *  RD4-RD7   LCD data bus        (gerenciado por biblioteca_lcd_2x16.h)
 *  RE0       LCD RW              (gerenciado pela biblioteca)
 *  RE1       LCD Enable          (gerenciado pela biblioteca)
 *  RE2       LCD RS              (gerenciado pela biblioteca)
 * ----------------------------------------------------------------
 */

/* ================================================================
 * INCLUDES
 * ================================================================ */
#include <p18f4550.h>          /* registradores e bits do PIC18F4550             */
#include <timers.h>            /* OpenTimer2() para configurar Timer2 (PWM)      */
#include <pwm.h>               /* OpenPWM1() / SetDCPWM1() para CCP1             */
#include <stdio.h>             /* sprintf() para formatar strings no LCD         */
#include "C:\PIC18\biblioteca_lcd_2x16.h"  /* lcd_posicao, imprime_string_lcd, etc */

/* ================================================================
 * CONFIG BITS  (gravados na flash de configuracao do PIC)
 * ================================================================ */
#pragma config FOSC   = HS        /* oscilador HS: cristal externo de 20 MHz       */
#pragma config CPUDIV = OSC1_PLL2 /* CPU roda em Fosc (PLL nao ativado = 20 MHz)   */
#pragma config WDT    = OFF       /* watchdog desativado (nao reinicia sozinho)     */
#pragma config BOR    = ON        /* brown-out reset ativo (protege contra queda Vcc)*/
#pragma config BORV   = 1         /* tensao de brown-out = 2,79 V                  */
#pragma config PBADEN = OFF       /* PORTB digital no reset (sem AN na inicializacao)*/
#pragma config LVP    = OFF       /* low-voltage programming desativado             */

/* ================================================================
 * CONSTANTES
 * ================================================================ */
#define TOTAL_ANDARES    6         /* numero de andares do elevador                 */
#define ANDAR_HOME       3         /* andar padrao de retorno apos inatividade      */
#define TEMP_MAX        31         /* limiar em grausC: acima disso -> SUPERAQUECIDO*/

#define TEMPO_PORTA    4000UL      /* ms: duracao total do ciclo de porta           */
                                   /*   0-1000ms  = "Abrindo porta"  (LED ON)      */
                                   /*   1000-2000ms= "Porta aberta!" (LED ON)      */
                                   /*   2000-3000ms= "Fechando porta"(LED apaga)   */
                                   /*   3000-4000ms= "Porta fechada" (LED OFF)     */
#define TEMPO_INATIVO  2300UL      /* ms sem chamada para retornar ao 3o andar      */
#define TEMPO_POR_ANDAR 3000UL     /* ms por andar percorrido (simulacao)           */
#define TEMPO_MSG_CHAMADO 1500UL   /* ms que "Chamado: Ax" fica visivel no LCD      */

/*
 * CALCULO DO PWM  (para o motor via L293D)
 *   Fosc = 20 MHz | Timer2 prescaler 1:4 | PR2 = 178
 *   Fpwm = Fosc / (4 x prescaler x (PR2+1))
 *        = 20.000.000 / (4 x 4 x 179) = 6.983 Hz  ~7 kHz        [OK]
 *
 *   Duty 38% (10 bits):
 *   DC_10bits = 0,38 x 4 x (PR2+1) = 0,38 x 716 = 272
 *   CCPR1L = 272 >> 2 = 68   |   DC1B1:DC1B0 = 272 & 0x03 = 0
 *   Duty real = 272 / 716 = 37,99%  ~38%                         [OK]
 */
#define PR2_VAL    178             /* valor de PR2 para ~7 kHz com prescaler 1:4    */
#define PWM_DUTY   272             /* valor de 10 bits equivalente a 38% duty cycle */

/* -- MACROS para os pinos de saida em PORTD ---------------------- */
#define MOTOR_IN1  LATDbits.LATD0  /* nivel logico de IN1 do L293D (direcao motor) */
#define MOTOR_IN2  LATDbits.LATD1  /* nivel logico de IN2 do L293D (direcao motor) */
#define PORTA_LED  LATDbits.LATD2  /* LED: 1=porta aberta, 0=porta fechada         */

/* ================================================================
 * TIPOS ENUMERADOS
 * ================================================================ */
/* Estado global da maquina de estados principal */
typedef enum {
    IDLE,           /* parado, aguardando chamada ou timeout de inatividade */
    MOVENDO,        /* indo para andar solicitado                           */
    PORTA_ABERTA,   /* chegou ao andar, porta aberta por TEMPO_PORTA ms     */
    SUPERAQUECIDO,  /* temperatura > TEMP_MAX, motor bloqueado              */
    RETORNANDO      /* voltando ao ANDAR_HOME por inatividade               */
} Estado;

/* Sentido atual de deslocamento do elevador */
typedef enum {
    PARADO,   /* elevador nao se move                                       */
    SUBINDO,  /* andares crescendo (andar_destino > andar_atual)            */
    DESCENDO  /* andares decrescendo (andar_destino < andar_atual)          */
} Direcao;

/* ================================================================
 * VARIAVEIS GLOBAIS
 * ================================================================ */
volatile unsigned long ms_tick = 0; /* contador de ms; incrementado a cada 1ms pela ISR */
                                    /* volatile = o compilador nao pode otimizar leitura */

Estado        estado        = IDLE;    /* estado atual da maquina de estados          */
Direcao       direcao       = PARADO;  /* direcao atual do elevador                   */
int           andar_atual   = 1;       /* andar onde o elevador esta fisicamente       */
int           andar_destino = 0;       /* andar para onde esta indo (0 = nenhum)       */
unsigned char fila[7]       = {0};     /* fila[1..6]: 1 = andar solicitado, 0 = nao   */
                                       /* fila[0] nao usado (andares comecam em 1)    */

unsigned long t_inativo = 0;  /* ms_tick em que a ultima acao ocorreu (ref. inatividade)*/
unsigned long t_porta   = 0;  /* ms_tick em que a porta comecou a abrir               */
unsigned long t_viagem  = 0;  /* ms_tick em que o movimento por andar comecou          */

unsigned int  adc_raw   = 0;       /* ultimo valor bruto lido do ADC (0 a 1023)        */
char          msg_chamado[17] = ""; /* buffer "Chamado: Ax" exibido ao acionar botao   */
unsigned long t_chamado       = 0;  /* ms_tick em que msg_chamado foi gerada            */

/*
 * BITMAPS CGRAM -- 4 caracteres customizados 5x8 pixels para animacao de setas
 *   char 0 (CGRAM 0x40): seta SUBINDO  frame A -- usada alternada com char 1
 *   char 1 (CGRAM 0x48): seta SUBINDO  frame B
 *   char 2 (CGRAM 0x50): seta DESCENDO frame A -- usada alternada com char 3
 *   char 3 (CGRAM 0x58): seta DESCENDO frame B
 * Cada linha do bitmap e um byte; bit4=col1, bit0=col5 (HD44780 5 colunas).
 */
const unsigned char BITMAP_UP_A[8]   = {0x04,0x0E,0x1F,0x04,0x04,0x04,0x04,0x00};
const unsigned char BITMAP_UP_B[8]   = {0x0E,0x1F,0x04,0x04,0x04,0x04,0x00,0x00};
const unsigned char BITMAP_DOWN_A[8] = {0x04,0x04,0x04,0x04,0x1F,0x0E,0x04,0x00};
const unsigned char BITMAP_DOWN_B[8] = {0x00,0x04,0x04,0x04,0x04,0x1F,0x0E,0x04};

/* ================================================================
 * PROTOTIPOS DAS FUNCOES
 * (declarados antes da ISR para evitar erro de compilacao [1111])
 * ================================================================ */
void init_hardware(void);
void init_pwm(void);
void init_adc(void);
void init_timer0(void);
void carregar_chars(void);
unsigned int ler_adc(void);
void ler_botoes(void);
int  proximo_fila(void);
void definir_direcao(void);
void motor_liga(void);
void motor_desliga(void);
void porta_abre(void);
void porta_fecha(void);
void maquina_estados(void);
void atualizar_lcd(void);
void ISR_TIMER0(void);

/* ================================================================
 * VETOR DE INTERRUPCAO ALTA PRIORIDADE (endereco fixo 0x0008)
 * O compilador C18 exige #pragma code para redirecionar para ISR_TIMER0.
 * ================================================================ */
#pragma code int_alta = 0x0008
void int_alta(void) { _asm GOTO ISR_TIMER0 _endasm }  /* salta para a ISR real */
#pragma code                                            /* volta ao segmento padrao */

/* ================================================================
 * ISR DO TIMER0 -- chamada a cada 1 ms
 *
 * Timer0: 16 bits | clock interno Fosc/4=5MHz | prescaler 1:8
 *   Cada tick do timer = 8 / 5.000.000 = 1,6 us
 *   Para 1 ms: 1.000 us / 1,6 us = 625 ticks
 *   Valor de recarga = 65536 - 625 = 64911 = 0xFD8F
 * ================================================================ */
#pragma interrupt ISR_TIMER0
void ISR_TIMER0(void) {
    ms_tick++;              /* incrementa contador global de milissegundos      */
    TMR0H = 0xFD;          /* recarrega byte alto do Timer0 (deve ser ANTES TMR0L) */
    TMR0L = 0x8F;          /* recarrega byte baixo -- 64911 = 65536-625 = 0xFD8F  */
    INTCONbits.TMR0IF = 0; /* limpa flag de overflow do Timer0 para proxima ISR  */
}

/* ================================================================
 *  MAIN
 * ================================================================ */
void main(void) {
    init_hardware();   /* configura TRIS, pull-ups, ADCON1 inicial              */
    init_pwm();        /* configura CCP1 e Timer2 para PWM ~7 kHz               */
    init_adc();        /* configura ADC: canal AN0, Fosc/16, resultado direita   */
    init_timer0();     /* configura Timer0 para gerar interrupcao a cada 1 ms   */

    /* inicia LCD em modo 4 bits, 2 linhas, cursor piscante, shift direita */
    lcd_inicia(0x28,   /* 0x28 = modo 4 bits, 2 linhas, fonte 5x8               */
               0x0F,   /* 0x0F = display ON, cursor ON, cursor piscante ON       */
               0x06);  /* 0x06 = incrementa posicao automaticamente, sem shift  */

    carregar_chars();  /* grava bitmaps de seta nos 4 primeiros chars da CGRAM  */

    /* configura Timer0 como interrupcao de alta prioridade */
    INTCON2bits.TMR0IP = 1; /* TMR0IP=1: Timer0 = alta prioridade                */
    INTCONbits.TMR0IF  = 0; /* limpa flag antes de habilitar (evita ISR espuria) */
    INTCONbits.TMR0IE  = 1; /* habilita a interrupcao do Timer0                  */
    RCONbits.IPEN      = 1; /* IPEN=1: ativa sistema de prioridades de interrupcao*/
    INTCONbits.GIEH    = 1; /* GIEH=1: habilita interrupcoes de alta prioridade  */

    t_inativo = ms_tick;   /* registra instante inicial para contagem de inatividade */

    /* laco principal -- nao bloqueia; usa ms_tick para toda temporizacao */
    while (1) {
        adc_raw = ler_adc();    /* le temperatura pelo trimpot ANAL0 / AN0 / RA0 */
        ler_botoes();           /* verifica RB0-RB5 e adiciona andares na fila    */
        maquina_estados();      /* avanca a maquina de estados do elevador        */
        atualizar_lcd();        /* atualiza LCD com andar, direcao e temperatura  */
    }
}

/* ================================================================
 * INIT_HARDWARE
 * Configura direcao dos pinos (TRIS) e estado inicial das saidas.
 * ADCON1 e configurado aqui apenas para desabilitar analogicos
 * globalmente; init_adc() depois habilita somente AN0.
 * ================================================================ */
void init_hardware(void) {
    ADCON1 = 0x0F;              /* PCFG=1111: todos os pinos do PORTA/PORTE digitais  */
                                /* (init_adc() vai reconfigurar para deixar AN0 analog)*/

    TRISB = 0xFF;               /* PORTB inteiro = entrada (RB0-RB5 sao botoes)       */
    INTCON2bits.RBPU = 0;       /* RBPU=0: habilita pull-ups internos do PORTB        */
                                /* (botoes ligam o pino ao GND; pull-up le HIGH=solto)*/

    TRISDbits.TRISD0 = 0;      /* RD0/MOTOR_IN1: saida (direcao L293D)               */
    TRISDbits.TRISD1 = 0;      /* RD1/MOTOR_IN2: saida (direcao L293D)               */
    TRISDbits.TRISD2 = 0;      /* RD2/PORTA_LED:  saida (LED indica porta aberta)     */
    LATD = 0x00;               /* inicializa PORTD com 0 (motor parado, LED apagado) */

    TRISCbits.TRISC2 = 0;      /* RC2/CCP1: saida PWM do motor (configurado em init_pwm)*/

    TRISAbits.TRISA0 = 1;      /* RA0/AN0: entrada analogica do trimpot de temperatura*/
                               /* DEPURACAO: se TRISA0=0 (saida), o ADC nao le nada  */
}

/* ================================================================
 * INIT_PWM
 * Configura CCP1 (RC2) em modo PWM usando Timer2.
 * Timer2: prescaler 1:4, postscaler 1:1, PR2 = 178
 *   -> Fpwm = 20MHz / (4 x 4 x 179) = 6.983 Hz ~7 kHz
 * SetDCPWM1(0) deixa motor desligado; motor_liga() chama SetDCPWM1(272).
 * ================================================================ */
void init_pwm(void) {
    OpenTimer2(TIMER_INT_OFF    /* Timer2 sem interrupcao (so base de tempo para PWM)*/
             & T2_PS_1_4        /* prescaler 1:4                                     */
             & T2_POST_1_1);    /* postscaler 1:1 (nao afeta PWM)                   */

    OpenPWM1(PR2_VAL);         /* define PR2=178 e coloca CCP1 em modo PWM          */
    SetDCPWM1(0);              /* duty cycle = 0%: motor desligado na inicializacao  */
}

/* ================================================================
 * INIT_ADC
 * Configura o conversor A/D para ler o trimpot em AN0 (RA0).
 *
 * DEPURACAO DO TRIMPOT:
 *   Se o valor nunca muda no Watch Window, cheque passo a passo:
 *   a) ADCON1 apos esta funcao deve ter PCFG = 0b1110 (AN0 analogico)
 *   b) ADCON0 deve ter ADON=1 e CHS=0000 (canal AN0 selecionado)
 *   c) ADCON2 deve ter ADCS=101 (Fosc/16) e ACQT=010 (4 TAD)
 *   d) TRISA0 deve ser 1 (entrada) -- feito em init_hardware()
 *   e) DIP switch ANAL0 (posicao 9, fileira SUPERIOR) deve estar ON
 * ================================================================ */
void init_adc(void) {
    /* ADCON1 = 0x8E (PICSimLab / PIC18F452):
     *   bit7      = ADFM = 1  -> resultado justificado a direita
     *               No PIC18F452, ADFM fica em ADCON1[7], nao em ADCON2.
     *               ADCON2 nao existe no PIC18F452; escrever nele nao tem efeito.
     *               Sem este bit, resultado sai left-justified (raw > 1023).
     *   bits 3:0  = PCFG = 1110 -> somente AN0 analogico, resto digital
     */
    ADCON1 = 0x8E;

    /* CHS=0000 (AN0), GO=0, ADON=1 */
    ADCON0 = 0x01;
}

/* ================================================================
 * INIT_TIMER0
 * Configura Timer0 em modo 16 bits para gerar overflow a cada 1 ms.
 *
 * Clock interno = Fosc/4 = 20MHz/4 = 5 MHz
 * Prescaler 1:8 -> tick do timer = 8 / 5.000.000 = 1,6 us
 * Para 1 ms: 1.000 us / 1,6 us = 625 ticks necessarios
 * Valor de recarga: 65536 - 625 = 64911 = 0xFD8F
 * ================================================================ */
void init_timer0(void) {
    T0CONbits.TMR0ON = 0;     /* desliga Timer0 antes de configurar               */
    T0CONbits.T08BIT = 0;     /* T08BIT=0: modo 16 bits (range 0-65535)           */
    T0CONbits.T0CS   = 0;     /* T0CS=0: clock interno (Fosc/4 = 5 MHz)           */
    T0CONbits.PSA    = 0;     /* PSA=0: prescaler habilitado (nao bypassa)        */
    T0CONbits.T0PS2  = 0;     /* \                                                */
    T0CONbits.T0PS1  = 1;     /*  > T0PS = 010: prescaler 1:8                    */
    T0CONbits.T0PS0  = 0;     /* /                                                */
    TMR0H = 0xFD;             /* byte alto do valor inicial (0xFD8F = 64911)      */
    TMR0L = 0x8F;             /* byte baixo -- TMR0H deve ser escrito ANTES TMR0L */
    T0CONbits.TMR0ON = 1;     /* liga Timer0; comeca a contar a partir de 0xFD8F  */
}

/* ================================================================
 * CARREGAR_CHARS
 * Grava os 4 bitmaps de seta na CGRAM do LCD HD44780.
 * CGRAM: enderecos 0x40-0x7F (8 chars x 8 bytes cada)
 *   char 0: 0x40-0x47  char 1: 0x48-0x4F
 *   char 2: 0x50-0x57  char 3: 0x58-0x5F
 * Para exibir: lcd_escreve_dado(0) imprime char 0, etc.
 * ================================================================ */
void carregar_chars(void) {
    unsigned char i;   /* indice para iterar sobre os 8 bytes de cada bitmap */

    /* char 0: seta SUBINDO frame A (SET CGRAM addr = 0x40) */
    lcd_envia_controle(0, 0, 0x40, 45);         /* posiciona CGRAM em 0x40          */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_UP_A[i]);   /* grava 8 bytes    */

    /* char 1: seta SUBINDO frame B (SET CGRAM addr = 0x48) */
    lcd_envia_controle(0, 0, 0x48, 45);         /* posiciona CGRAM em 0x48          */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_UP_B[i]);

    /* char 2: seta DESCENDO frame A (SET CGRAM addr = 0x50) */
    lcd_envia_controle(0, 0, 0x50, 45);         /* posiciona CGRAM em 0x50          */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_DOWN_A[i]);

    /* char 3: seta DESCENDO frame B (SET CGRAM addr = 0x58) */
    lcd_envia_controle(0, 0, 0x58, 45);         /* posiciona CGRAM em 0x58          */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_DOWN_B[i]);

    lcd_posicao(1, 1); /* retorna cursor para DDRAM linha 1, coluna 1 (imprescindivel)*/
                       /* sem isto o proximo lcd_posicao pode falhar                 */
}

/* ================================================================
 * LER_ADC
 * Inicia conversao no canal AN0 e aguarda o flag ADIF.
 * Retorna valor de 10 bits (0 a 1023).
 * Mapeamento para temperatura: temp_graus = adc_raw * 50 / 1023
 *   0    -> 0  grausC (trimpot na posicao minima)
 *   1023 -> 50 grausC (trimpot na posicao maxima)
 *
 * DEPURACAO DO TRIMPOT:
 *   - Coloque breakpoint no `return` desta funcao.
 *   - Gire o trimpot e execute novamente; ADRESH:ADRESL devem mudar.
 *   - Se sempre retornar 0: PCFG errado (AN0 nao e analogico).
 *   - Se sempre retornar 1023: curto no pino ou tensao fixa em VDD.
 *   - Se ADIF nunca sobe: ADON=0 ou clock ADC errado (ADCS).
 *   - PICSimLab: o trimpot ANAL0 (circulo BRANCO no centro da placa) controla RA0.
 *     Scroll para cima = tensao maior = valor maior.
 * ================================================================ */
unsigned int ler_adc(void) {
    /* PIC18F452 (PICSimLab):
     *   GO fica em bit2 do ADCON0 (nao bit1 como no PIC18F4550).
     *   ADCON0bits.GO = 1 setava o bit1 -> conversao nao disparava corretamente.
     *   while(ADIF) ou while(GO) travavam -> usar Delay10KTCYx(1) no lugar. */
    ADCON0 |= 0x04;      /* bit2 = GO no PIC18F452 -> dispara conversao            */
    Delay10KTCYx(1);     /* ~2 ms >> 9 us necessarios; nao esperar ADIF (trava)    */
    return (((unsigned int)ADRESH << 8) | ADRESL);
}

/* ================================================================
 * LER_BOTOES
 * Varre RB0-RB5 (botoes dos andares 1-6) com pull-up interno.
 * Logica ativa em LOW: pino = 0 quando botao pressionado.
 * Debounce por tempo: aceita o acionamento somente se >= 20 ms
 * se passaram desde a ultima aceitacao do mesmo andar.
 * ================================================================ */
static unsigned long t_deb[7] = {0}; /* ultimo ms_tick aceito por andar [1..6]       */

void ler_botoes(void) {
    unsigned char b;      /* indice do bit (0 a 5)                                   */
    unsigned char andar;  /* numero do andar (1 a 6)                                 */
    unsigned char snap = PORTB; /* snapshot atomico de PORTB -- evita leitura dupla  */
                                /* se interrupcao mudar PORTB entre duas leituras     */

    for (b = 0; b < 6; b++) {            /* percorre os 6 bits dos botoes            */
        andar = b + 1;                    /* botao RBb corresponde ao andar b+1       */

        if (!(snap & (unsigned char)(1 << b))) {  /* LOW = botao pressionado         */
            if ((ms_tick - t_deb[andar]) >= 20) { /* passou mais de 20 ms?           */
                t_deb[andar] = ms_tick;            /* registra instante aceito         */

                if (andar != (unsigned char)andar_atual) { /* outro andar: enfileira  */
                    fila[andar]  = 1;              /* marca andar na fila de chamadas  */
                    t_inativo    = ms_tick;        /* reinicia contador de inatividade */
                    sprintf(msg_chamado,           /* formata mensagem para o LCD      */
                            "Chamado: A%d     ",   /* ex.: "Chamado: A3     "          */
                            (int)andar);
                    t_chamado = ms_tick;           /* registra quando a mensagem nasceu*/
                } else if (estado == IDLE) {       /* mesmo andar e parado: abre porta */
                    t_inativo = ms_tick;
                    porta_abre();                  /* acende LED RD2                   */
                    t_porta   = ms_tick;           /* inicia ciclo de porta            */
                    estado    = PORTA_ABERTA;      /* transicao direta para porta aberta*/
                    sprintf(msg_chamado, "Chamado: A%d     ", (int)andar);
                    t_chamado = ms_tick;
                }
            }
        }
    }
}

/* ================================================================
 * PROXIMO_FILA
 * Algoritmo SCAN: prioriza andares na direcao de movimento atual.
 * Equivale ao escalonamento usado em elevadores reais.
 * Retorna o numero do proximo andar a atender (1-6) ou 0 se fila vazia.
 * ================================================================ */
int proximo_fila(void) {
    int i;

    if (direcao == SUBINDO || direcao == PARADO) {
        /* subindo: busca primeiro acima do andar atual */
        for (i = andar_atual + 1; i <= TOTAL_ANDARES; i++)
            if (fila[i]) return i;  /* retorna o mais proximo acima               */
        /* nenhum acima: busca abaixo (inverte sentido) */
        for (i = andar_atual - 1; i >= 1; i--)
            if (fila[i]) return i;  /* retorna o mais proximo abaixo              */
    } else {
        /* descendo: busca primeiro abaixo do andar atual */
        for (i = andar_atual - 1; i >= 1; i--)
            if (fila[i]) return i;  /* retorna o mais proximo abaixo              */
        /* nenhum abaixo: busca acima (inverte sentido) */
        for (i = andar_atual + 1; i <= TOTAL_ANDARES; i++)
            if (fila[i]) return i;  /* retorna o mais proximo acima               */
    }
    return 0;   /* fila completamente vazia                                       */
}

/* ================================================================
 * DEFINIR_DIRECAO
 * Atualiza a variavel `direcao` com base em andar_atual e andar_destino.
 * Chamada sempre antes de motor_liga() para garantir sentido correto.
 * ================================================================ */
void definir_direcao(void) {
    if      (andar_destino > andar_atual) direcao = SUBINDO;   /* vai para cima  */
    else if (andar_destino < andar_atual) direcao = DESCENDO;  /* vai para baixo */
    else                                  direcao = PARADO;    /* ja esta la     */
}

/* ================================================================
 * MOTOR_LIGA / MOTOR_DESLIGA
 * Controla a ponte H L293D via IN1/IN2 (RD0/RD1) e PWM (CCP1/RC2).
 * Sentido:
 *   SUBINDO:  IN1=1, IN2=0 -> motor gira em um sentido
 *   DESCENDO: IN1=0, IN2=1 -> motor gira no sentido inverso
 * SetDCPWM1(272) aplica 38% duty cycle; SetDCPWM1(0) corta a potencia.
 * ================================================================ */
void motor_liga(void) {
    if (direcao == SUBINDO) {
        MOTOR_IN1 = 1;         /* IN1=1: define sentido de subida no L293D        */
        MOTOR_IN2 = 0;         /* IN2=0: complementar de IN1                      */
    } else {
        MOTOR_IN1 = 0;         /* IN1=0: define sentido de descida no L293D       */
        MOTOR_IN2 = 1;         /* IN2=1: complementar de IN1                      */
    }
    SetDCPWM1(PWM_DUTY);       /* aplica 38% duty cycle no CCP1 (RC2) -> motor gira*/
}

void motor_desliga(void) {
    SetDCPWM1(0);              /* duty cycle = 0%: sinal PWM cai para LOW           */
    MOTOR_IN1 = 0;             /* IN1=0: sem tensao no L293D                        */
    MOTOR_IN2 = 0;             /* IN2=0: sem tensao no L293D (freio eletrico)       */
}

/* ================================================================
 * PORTA_ABRE / PORTA_FECHA
 * Controla o LED em RD2 que simula a porta do elevador.
 * LED aceso = porta aberta | LED apagado = porta fechada.
 * ================================================================ */
void porta_abre(void)  { PORTA_LED = 1; }   /* acende LED RD2 (porta aberta)       */
void porta_fecha(void) { PORTA_LED = 0; }   /* apaga  LED RD2 (porta fechada)      */

/* ================================================================
 * MAQUINA_ESTADOS
 * Nucleo logico do elevador. Chamada a cada iteracao do while(1).
 * Nao tem delays internos: usa (ms_tick - t_xxx) para medir tempo.
 *
 * Conversao ADC -> temperatura:
 *   temp (grausC) = adc_raw * 50 / 1023
 *   Ex.: adc_raw = 640 -> temp = 640*50/1023 = 31,28 -> 31 grausC
 *
 * DEPURACAO DO TRIMPOT:
 *   Para testar o estado SUPERAQUECIDO, gire o trimpot ate a posicao
 *   maxima (scroll do mouse para cima no PICSimLab).
 *   adc_raw > 636 corresponde a temp > 31 grausC.
 *   636 = 31 * 1023 / 50 = 31 * 20,46 = 634 (aproximado)
 * ================================================================ */
void maquina_estados(void) {
    /* converte adc_raw em temperatura inteira (sem float, usando inteiros) */
    int temp = (int)((unsigned long)adc_raw * 50UL / 1023UL);
    /* multiplicacao por 50UL antes da divisao evita overflow de unsigned int */

    switch (estado) {

        /* ---- IDLE: elevador parado, aguardando chamada ---- */
        case IDLE:
            motor_desliga();   /* garante motor desligado no estado IDLE            */

            if (temp > TEMP_MAX) {          /* temperatura acima do limite?         */
                estado = SUPERAQUECIDO;     /* bloqueia o elevador imediatamente    */
                break;
            }

            andar_destino = proximo_fila(); /* busca proximo andar da fila (SCAN)   */
            if (andar_destino != 0) {       /* existe chamada pendente?             */
                definir_direcao();          /* calcula SUBINDO ou DESCENDO          */
                motor_liga();              /* aciona motor no sentido correto        */
                t_viagem  = ms_tick;       /* registra inicio do deslocamento       */
                t_inativo = ms_tick;       /* reinicia timeout de inatividade       */
                estado    = MOVENDO;       /* transicao para estado MOVENDO         */
                break;
            }

            /* nenhuma chamada: verifica timeout de inatividade */
            if ((ms_tick - t_inativo) >= TEMPO_INATIVO   /* 2,3 s sem acao?        */
                    && andar_atual != ANDAR_HOME) {       /* e nao esta no 3o andar?*/
                andar_destino = ANDAR_HOME;  /* destino = 3o andar                 */
                definir_direcao();           /* calcula direcao para o 3o           */
                motor_liga();               /* aciona motor                         */
                t_viagem  = ms_tick;        /* registra inicio da viagem de retorno */
                t_inativo = ms_tick;        /* reinicia timeout                     */
                estado    = RETORNANDO;     /* transicao para RETORNANDO            */
            }
            break;

        /* ---- MOVENDO / RETORNANDO: avanca 1 andar por TEMPO_POR_ANDAR ---- */
        case MOVENDO:
        case RETORNANDO:
            if (temp > TEMP_MAX) {          /* temperatura sobe durante viagem?     */
                motor_desliga();            /* para imediatamente                   */
                estado = SUPERAQUECIDO;     /* bloqueia elevador                    */
                break;
            }

            if ((ms_tick - t_viagem) >= TEMPO_POR_ANDAR) { /* passou 3 s?          */
                t_viagem = ms_tick;         /* reinicia cronometro do proximo andar  */

                if (direcao == SUBINDO)  andar_atual++;  /* avanca 1 andar para cima*/
                if (direcao == DESCENDO) andar_atual--;  /* avanca 1 andar para baixo*/

                if (andar_atual == andar_destino) {  /* chegou ao destino?          */
                    fila[andar_destino] = 0; /* remove andar da fila de chamadas    */
                    motor_desliga();         /* para o motor                         */
                    direcao = PARADO;        /* zera direcao: LCD mostra PARADO, nao seta*/
                    porta_abre();            /* acende LED (simula abertura de porta)*/
                    t_porta = ms_tick;       /* registra inicio do ciclo de porta    */
                    estado  = PORTA_ABERTA;  /* transicao para PORTA_ABERTA          */
                }
            }
            break;

        /* ---- PORTA_ABERTA: 4 fases de 1 s cada ---- */
        case PORTA_ABERTA:
            if ((ms_tick - t_porta) >= 2000UL) porta_fecha(); /* apaga LED na fase fechando*/
            if ((ms_tick - t_porta) >= TEMPO_PORTA) { /* passou 4 s?               */
                t_inativo = ms_tick; /* reinicia timeout de inatividade             */
                estado    = IDLE;   /* volta ao estado de espera                    */
            }
            break;

        /* ---- SUPERAQUECIDO: motor bloqueado ate temperatura cair ---- */
        case SUPERAQUECIDO:
            motor_desliga();        /* garante motor desligado enquanto superaquecido*/
            direcao = PARADO;       /* seta direcao como PARADO (LCD exibe STOP)    */

            if (temp <= TEMP_MAX) { /* temperatura normalizou? (girou trimpot p/ baixo)*/
                t_inativo = ms_tick; /* reinicia inatividade apos resfriamento       */
                if (andar_destino != 0 && andar_destino != andar_atual) {
                    /* havia viagem em andamento antes do superaquecimento */
                    definir_direcao(); /* recalcula direcao (estado atual pode ter mudado)*/
                    motor_liga();      /* retoma viagem de onde parou               */
                    t_viagem = ms_tick; /* reinicia cronometro do andar atual       */
                    estado   = MOVENDO; /* volta a mover                            */
                } else {
                    estado = IDLE;     /* nao havia destino: volta a esperar        */
                }
            }
            break;
    }
}

/* ================================================================
 * ATUALIZAR_LCD
 * Atualiza o display LCD a cada iteracao do while(1).
 *
 * LINHA 1 (16 chars): "Andar: X  [seta] "
 *   - seta animada (CGRAM char 0/1 subindo, 2/3 descendo)
 *   - "STOP  " quando SUPERAQUECIDO
 *
 * LINHA 2 (16 chars): mensagem de estado ou temperatura
 *   - prioridade: "Chamado: Ax" por TEMPO_MSG_CHAMADO ms apos botao
 *   - SUPERAQUECIDO: "Temp elevada!   "
 *   - PORTA_ABERTA:  "Abrindo porta   " / "Porta aberta!   " / "Fechando porta  "
 *   - MOVENDO:       "Subindo...." ou "Descendo...." (animado por pontos)
 *   - IDLE:          "Tmp: XX.XC      " (temperatura com 1 decimal)
 *
 * Temperatura com 1 decimal (sem float):
 *   temp_10 = adc_raw * 500 / 1023  (decimos de grau)
 *   ex.: adc_raw=640 -> temp_10=312 -> "31.2C"
 * ================================================================ */
void atualizar_lcd(void) {
    char buf[18];               /* buffer para sprintf (max 16 chars + nulo + margem)*/
    unsigned int temp_10;       /* temperatura em decimos de grau (sem float)        */
    unsigned long elapsed;      /* tempo decorrido desde abertura da porta (ms)      */
    static unsigned long t_anim = 0; /* ms_tick da ultima troca de frame de animacao */
    static unsigned long t_lcd  = 0; /* ms_tick da ultima atualizacao do LCD         */
    static unsigned char frame  = 0; /* indice do frame atual: 0, 1, 2 ou 3         */

    /* throttle: atualiza LCD no maximo a cada 100 ms                               */
    /* sem isso, lcd_posicao() e chamado milhares de vezes/s -> piscar preto        */
    if ((ms_tick - t_lcd) < 100UL) return;
    t_lcd = ms_tick;

    /* troca de frame a cada 350 ms -- cria efeito de animacao nas setas e pontos */
    if ((ms_tick - t_anim) >= 350UL) { /* passaram 350 ms desde a ultima troca?     */
        t_anim = ms_tick;              /* atualiza referencia de tempo               */
        frame  = (unsigned char)((frame + 1) & 0x03); /* cicla: 0->1->2->3->0       */
    }

    /* temperatura em decimos: *500 antes de /1023 evita perda de precisao inteira */
    temp_10 = (unsigned int)((unsigned long)adc_raw * 500UL / 1023UL);

    /* ---- LINHA 1: numero do andar + indicador de direcao ---- */
    lcd_posicao(1, 1);                 /* posiciona cursor na linha 1, coluna 1     */
    sprintf(buf, "Andar: %d  ", andar_atual); /* ex.: "Andar: 3  " (10 chars)       */
    imprime_buffer_lcd(buf, 10);       /* imprime exatamente 10 chars               */

    if (estado == SUPERAQUECIDO) {
        imprime_string_lcd("STOP  "); /* 6 chars -> total linha 1 = 16 chars        */
    } else if (direcao == SUBINDO) {
        /* alterna char CGRAM 0 e 1 (UP_A e UP_B) -- seta pulsando para cima */
        lcd_escreve_dado((unsigned char)(frame & 1)); /* frame&1: 0 ou 1            */
        imprime_string_lcd("     "); /* 5 espacos para completar 16 chars totais    */
    } else if (direcao == DESCENDO) {
        /* alterna char CGRAM 2 e 3 (DOWN_A e DOWN_B) -- seta pulsando para baixo */
        lcd_escreve_dado((unsigned char)(2 + (frame & 1))); /* 2 ou 3              */
        imprime_string_lcd("     "); /* 5 espacos para completar 16 chars totais    */
    } else {
        imprime_string_lcd("PARADO"); /* parado: indicador visual de elevador parado*/
    }

    /* ---- LINHA 2: mensagem de estado ---- */
    lcd_posicao(2, 1);                 /* posiciona cursor na linha 2, coluna 1     */

    /* prioridade maxima: exibe "Chamado: Ax" por TEMPO_MSG_CHAMADO ms */
    if (msg_chamado[0] != '\0'                             /* mensagem existe?      */
            && (ms_tick - t_chamado) < TEMPO_MSG_CHAMADO) { /* ainda dentro do tempo?*/
        imprime_buffer_lcd(msg_chamado, 16); /* imprime exatamente 16 chars         */
        return; /* sai sem sobrescrever com outra mensagem                          */
    }

    switch (estado) {
        case SUPERAQUECIDO:
            imprime_string_lcd("Temp elevada!   "); /* 16 chars (3 espacos finais)  */
            break;

        case PORTA_ABERTA:
            elapsed = ms_tick - t_porta;  /* tempo desde abertura da porta em ms   */
            if      (elapsed < 1000UL) imprime_string_lcd("Abrindo porta   ");
            else if (elapsed < 2000UL) imprime_string_lcd("Porta aberta!   ");
            else if (elapsed < 3000UL) imprime_string_lcd("Fechando porta  ");
            else                       imprime_string_lcd("Porta fechada   ");
            break;

        case MOVENDO:
        case RETORNANDO:
            if (direcao == SUBINDO) {
                /* pontos animados sincronizados com frame (0-1-2-3) */
                switch (frame) {
                    case 0:  imprime_string_lcd("Subindo.        "); break;
                    case 1:  imprime_string_lcd("Subindo..       "); break;
                    case 2:  imprime_string_lcd("Subindo...      "); break;
                    default: imprime_string_lcd("Subindo....     "); break;
                }
            } else {
                switch (frame) {
                    case 0:  imprime_string_lcd("Descendo.       "); break;
                    case 1:  imprime_string_lcd("Descendo..      "); break;
                    case 2:  imprime_string_lcd("Descendo...     "); break;
                    default: imprime_string_lcd("Descendo....    "); break;
                }
            }
            break;

        case IDLE:
        default:
            /* exibe temperatura com 1 casa decimal: temp_10=312 -> "31.2C" */
            sprintf(buf, "Tmp: %u.%uC       ",
                    temp_10 / 10,   /* parte inteira (ex.: 312/10 = 31)            */
                    temp_10 % 10);  /* primeira casa decimal (ex.: 312%10 = 2)     */
            imprime_buffer_lcd(buf, 16); /* imprime exatamente 16 chars            */
            break;
    }
}
