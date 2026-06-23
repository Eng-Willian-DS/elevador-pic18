/*
 * Trabalho -- Central para Elevador (6 andares)
 * ELE1012 -- Microcontroladores -- UTFPR Medianeira
 * PIC18F4550 | Fosc = 20 MHz | MPLAB v8.92 + C18 v3.47 | PICSimLab
 *
 * -- CONEXOES ----------------------------------------------------
 *  RA0/AN0   potenciometro ANAL0  (simula temperatura 0-50  grausC; DIP AN0 ON -- trimpot ao lado LCD)
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

#include <p18f4550.h>
#include <timers.h>
#include <pwm.h>
#include <stdio.h>
#include "C:\PIC18\biblioteca_lcd_2x16.h"

/* -- CONFIG BITS (igual aos exemplos da disciplina) ------------ */
#pragma config FOSC   = HS          /* oscilador HS -- cristal 20 MHz        */
#pragma config CPUDIV = OSC1_PLL2   /* CPU = Fosc (PLL nao usado)           */
#pragma config WDT    = OFF
#pragma config BOR    = ON
#pragma config BORV   = 1
#pragma config PBADEN = OFF         /* PORTB digital no reset               */
#pragma config LVP    = OFF

/* -- CONSTANTES ------------------------------------------------ */
#define TOTAL_ANDARES    6
#define ANDAR_HOME       3          /* andar de retorno apos inatividade     */
#define TEMP_MAX        31          /* graus C -- elev. para acima deste val  */

#define TEMPO_PORTA    3000UL       /* ms -- 1s abrindo + 1s aberta + 1s fechando (enunciado: 1s) */
#define TEMPO_INATIVO  2300UL       /* ms -- retorna ao 3o apos 2,3 s inativo */
#define TEMPO_POR_ANDAR 3000UL      /* ms -- simulacao: 3 s por andar         */
#define TEMPO_MSG_CHAMADO 1500UL    /* ms -- exibe "Chamado: Ax" no LCD       */

/*
 * PWM -- calculo:
 *   Fosc = 20 MHz  |  Timer2 prescaler 1:4  |  PR2 = 178
 *   Fpwm = 20e6 / (4 x 4 x (178+1)) = 6.983 kHz  ~ 7 kHz     [OK]
 *
 *   Duty 38% (valor 10 bits):
 *   DC = int(0,38 x 4 x (178+1)) = int(272,08) = 272
 *   CCPR1L = 272 >> 2 = 68  |  DC1B1:DC1B0 = 272 & 0x03 = 0
 *   Duty real = 272 / 716 = 37,99%  ~ 38%                      [OK]
 */
#define PR2_VAL    178
#define PWM_DUTY   272              /* valor 10 bits para 38% duty cycle     */

/* pinos de saida em PORTD (motor L293D + LED de porta) */
#define MOTOR_IN1  LATDbits.LATD0
#define MOTOR_IN2  LATDbits.LATD1
#define PORTA_LED  LATDbits.LATD2

/* -- TIPOS ---------------------------------------------------- */
typedef enum { IDLE, MOVENDO, PORTA_ABERTA, SUPERAQUECIDO, RETORNANDO } Estado;
typedef enum { PARADO, SUBINDO, DESCENDO }                               Direcao;

/* -- VARIAVEIS GLOBAIS ---------------------------------------- */
volatile unsigned long ms_tick = 0; /* incrementado a cada 1 ms na ISR      */

Estado        estado        = IDLE;
Direcao       direcao       = PARADO;
int           andar_atual   = 1;
int           andar_destino = 0;
unsigned char fila[7]       = {0};  /* fila[1..6]: 1 = andar solicitado     */

unsigned long t_inativo = 0;
unsigned long t_porta   = 0;
unsigned long t_viagem  = 0;

unsigned int  adc_raw   = 0;        /* leitura bruta ADC (0-1023)           */
char          msg_chamado[17] = ""; /* mensagem "Chamado: Ax" para o LCD    */
unsigned long t_chamado       = 0;

/*
 * Bitmaps 5x8 -- 4 chars CGRAM para animacao de setas:
 *   char 0: ^ frame A  char 1: ^ frame B  (alternados = movimento para cima)
 *   char 2: v frame A  char 3: v frame B  (alternados = movimento para baixo)
 */
const unsigned char BITMAP_UP_A[8]   = {0x04,0x0E,0x1F,0x04,0x04,0x04,0x04,0x00};
const unsigned char BITMAP_UP_B[8]   = {0x0E,0x1F,0x04,0x04,0x04,0x04,0x00,0x00};
const unsigned char BITMAP_DOWN_A[8] = {0x04,0x04,0x04,0x04,0x1F,0x0E,0x04,0x00};
const unsigned char BITMAP_DOWN_B[8] = {0x00,0x04,0x04,0x04,0x04,0x1F,0x0E,0x04};

/* -- PROTOTIPOS ----------------------------------------------- */
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

/* -- VETOR DE INTERRUPCAO (endereco 0x0008 = alta prioridade) -- */
#pragma code int_alta = 0x0008
void int_alta(void) { _asm GOTO ISR_TIMER0 _endasm }
#pragma code

/*
 * ISR do Timer0 -- dispara a cada 1 ms
 *
 * Timer0: 16 bits, clock interno (Fosc/4 = 5 MHz), prescaler 1:8
 *   Tck = 8 / 5.000.000 = 1,6 us por tick
 *   Para 1 ms: 1000 us / 1,6 us = 625 ticks
 *   Valor de recarga = 65536 - 625 = 64911 = 0xFD8F
 */
#pragma interrupt ISR_TIMER0
void ISR_TIMER0(void) {
    ms_tick++;
    TMR0H = 0xFD;           /* recarga para proxima interrupcao em 1 ms     */
    TMR0L = 0x8F;
    INTCONbits.TMR0IF = 0;  /* limpa flag de overflow                       */
}

/* ===============================================================
 *  MAIN
 * =============================================================== */
void main(void) {
    init_hardware();
    init_pwm();
    init_adc();
    init_timer0();

    /* inicia LCD: 4 bits | 2 linhas | cursor piscante | shift direita */
    lcd_inicia(0x28, 0x0F, 0x06);
    carregar_chars();

    /* habilita Timer0 como interrupcao de alta prioridade */
    INTCON2bits.TMR0IP = 1;
    INTCONbits.TMR0IF  = 0;
    INTCONbits.TMR0IE  = 1;
    RCONbits.IPEN      = 1;  /* sistema de prioridades ativo                */
    INTCONbits.GIEH    = 1;  /* habilita interrupcoes de alta prioridade    */

    t_inativo = ms_tick;     /* inicio do contador de inatividade           */

    while (1) {
        adc_raw = ler_adc();
        ler_botoes();
        maquina_estados();
        atualizar_lcd();
    }
}

/* -- INIT HARDWARE -------------------------------------------- */
void init_hardware(void) {
    /* desabilita todas as entradas analogicas (configura digitais) */
    ADCON1 = 0x0F;              /* PCFG = 1111: todos digitais por padrao   */

    /* PORTB RB0-RB5: entradas (botoes dos andares) com pull-up interno     */
    TRISB = 0xFF;
    INTCON2bits.RBPU = 0;       /* 0 = habilita pull-ups internos do PORTB  */

    /* PORTD: RD0/RD1/RD2 = saidas (motor + LED porta)                     */
    TRISDbits.TRISD0 = 0;       /* MOTOR_IN1: saida                         */
    TRISDbits.TRISD1 = 0;       /* MOTOR_IN2: saida                         */
    TRISDbits.TRISD2 = 0;       /* PORTA_LED: saida                         */
    LATD = 0x00;

    /* RC2: saida PWM (CCP1) -- configurado pelo init_pwm()                  */
    TRISCbits.TRISC2 = 0;

    /* RA0: entrada analogica AN0 (potenciometro de temperatura)            */
    TRISAbits.TRISA0 = 1;
}

/* -- INIT PWM ------------------------------------------------- */
/*
 * Usa CCP1 (RC2) em modo Single Output com Timer2.
 * Timer2: prescaler 1:4, postscaler 1:1, PR2 = 178
 *   -> Fpwm = 20 MHz / (4 x 4 x 179) ~ 6.983 Hz ~ 7 kHz
 * Duty cycle 38%: SetDCPWM1(272)
 *   -> CCPR1L = 68, DC1B1:DC1B0 = 0
 *   -> Duty real = 272/716 = 37,99% ~ 38%
 */
void init_pwm(void) {
    OpenTimer2(TIMER_INT_OFF & T2_PS_1_4 & T2_POST_1_1);
    OpenPWM1(PR2_VAL);          /* define PR2 = 178                         */
    SetDCPWM1(0);               /* motor desligado na inicializacao          */
}

/* -- INIT ADC ------------------------------------------------- */
/*
 * Canal AN0 (RA0) -- potenciometro simulando temperatura.
 * Clock de conversao: Fosc/16 -> TAD = 0,8 us (minimo para 20 MHz)
 * Aquisicao automatica: 4 TAD = 3,2 us
 * Resultado justificado a direita (ADFM=1): ADRESH:ADRESL = 0..1023
 */
void init_adc(void) {
    /* tensao de referencia = VDD/VSS (interno)                             */
    ADCON1bits.VCFG1 = 0;
    ADCON1bits.VCFG0 = 0;
    /* PCFG = 1110: somente AN0 analogico, demais digitais                  */
    ADCON1bits.PCFG3 = 1;
    ADCON1bits.PCFG2 = 1;
    ADCON1bits.PCFG1 = 1;
    ADCON1bits.PCFG0 = 0;

    /* clock de conversao: Fosc/16 (ADCS = 101)                            */
    ADCON2bits.ADCS2 = 1;
    ADCON2bits.ADCS1 = 0;
    ADCON2bits.ADCS0 = 1;
    /* tempo de aquisicao: 4 TAD (ACQT = 010)                              */
    ADCON2bits.ACQT2 = 0;
    ADCON2bits.ACQT1 = 1;
    ADCON2bits.ACQT0 = 0;
    /* resultado justificado a direita                                      */
    ADCON2bits.ADFM  = 1;

    /* seleciona canal AN0 (ANAL0/RA0) -- compativel com PIC18F452         */
    ADCON0 = 0x01;              /* CHS=0000 (AN0), ADON=1                   */
}

/* -- INIT TIMER0 ---------------------------------------------- */
/*
 * Timer0 em modo 16 bits, clock interno, prescaler 1:8.
 * Cada tick = 8 x (4/20 MHz) = 1,6 us
 * Para 1 ms: 1000 us / 1,6 us = 625 ticks
 * Recarga: 65536 - 625 = 64911 = 0xFD8F
 */
void init_timer0(void) {
    T0CONbits.TMR0ON = 0;
    T0CONbits.T08BIT = 0;       /* modo 16 bits                             */
    T0CONbits.T0CS   = 0;       /* clock interno (Fosc/4 = 5 MHz)           */
    T0CONbits.PSA    = 0;       /* prescaler habilitado                     */
    T0CONbits.T0PS2  = 0;       /* prescaler 1:8  (T0PS = 010)              */
    T0CONbits.T0PS1  = 1;
    T0CONbits.T0PS0  = 0;
    TMR0H = 0xFD;
    TMR0L = 0x8F;
    T0CONbits.TMR0ON = 1;
}

/* -- CHARS CUSTOMIZADOS (CGRAM) ------------------------------- */
/*
 * HD44780 suporta ate 8 caracteres de 5x8 pixels na CGRAM.
 * Cada caracter ocupa 8 bytes consecutivos:
 *   char 0: CGRAM 0x40-0x47
 *   char 1: CGRAM 0x48-0x4F
 * Para exibir: lcd_escreve_dado(0) ou lcd_escreve_dado(1)
 */
void carregar_chars(void) {
    unsigned char i;

    lcd_envia_controle(0, 0, 0x40, 45); /* char 0: seta SUBINDO  frame A    */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_UP_A[i]);

    lcd_envia_controle(0, 0, 0x48, 45); /* char 1: seta SUBINDO  frame B    */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_UP_B[i]);

    lcd_envia_controle(0, 0, 0x50, 45); /* char 2: seta DESCENDO frame A    */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_DOWN_A[i]);

    lcd_envia_controle(0, 0, 0x58, 45); /* char 3: seta DESCENDO frame B    */
    for (i = 0; i < 8; i++) lcd_escreve_dado(BITMAP_DOWN_B[i]);

    lcd_posicao(1, 1);                  /* volta cursor para DDRAM          */
}

/* -- LER ADC -------------------------------------------------- */
/*
 * Inicia conversao no canal AN0 e aguarda conclusao (~12 us).
 * Retorna valor bruto de 10 bits (0-1023).
 * Mapeamento: 0 = 0  grausC | 1023 = 50  grausC (faixa do potenciometro)
 */
unsigned int ler_adc(void) {
    unsigned int tmo = 0;
    ADCON0bits.GO = 1;
    while (!PIR1bits.ADIF && ++tmo);  /* timeout: PIC18F452 nao tem ADCON2  */
    PIR1bits.ADIF = 0;
    return (((unsigned int)ADRESH << 8) | ADRESL);
}

/* -- LER BOTOES (RB0-RB5 = andares 1-6, pull-up, ativo em LOW) - */
/*
 * RB0=A1  RB1=A2  RB2=A3  RB3=A4  RB4=A5  RB5=A6
 * Debounce por tempo: aceita acionamento so se passaram >=20 ms
 * desde o ultimo acionamento aceito (evita bouncing mecanico).
 */
static unsigned long t_deb[7] = {0};

void ler_botoes(void) {
    unsigned char b, andar;
    unsigned char snap = PORTB;   /* snapshot atomico -- evita leitura dupla   */

    for (b = 0; b < 6; b++) {
        andar = b + 1;
        if (!(snap & (unsigned char)(1 << b))) { /* LOW = botao pressionado   */
            if ((ms_tick - t_deb[andar]) >= 20) {
                t_deb[andar] = ms_tick;
                if (andar != (unsigned char)andar_atual) {
                    fila[andar]  = 1;
                    t_inativo    = ms_tick;
                    sprintf(msg_chamado, "Chamado: A%d     ", (int)andar);
                    t_chamado    = ms_tick;
                }
            }
        }
    }
}

/* -- PROXIMO ANDAR (algoritmo SCAN) -------------------------- */
/*
 * Prioriza andares na direcao atual antes de inverter o sentido.
 * Equivale ao algoritmo usado em elevadores reais (SCAN/LOOK).
 */
int proximo_fila(void) {
    int i;
    if (direcao == SUBINDO || direcao == PARADO) {
        for (i = andar_atual + 1; i <= TOTAL_ANDARES; i++)
            if (fila[i]) return i;
        for (i = andar_atual - 1; i >= 1; i--)
            if (fila[i]) return i;
    } else {
        for (i = andar_atual - 1; i >= 1; i--)
            if (fila[i]) return i;
        for (i = andar_atual + 1; i <= TOTAL_ANDARES; i++)
            if (fila[i]) return i;
    }
    return 0;   /* fila vazia */
}

/* -- DEFINIR DIRECAO ------------------------------------------ */
void definir_direcao(void) {
    if      (andar_destino > andar_atual) direcao = SUBINDO;
    else if (andar_destino < andar_atual) direcao = DESCENDO;
    else                                  direcao = PARADO;
}

/* -- MOTOR ---------------------------------------------------- */
/*
 * O sinal PWM (~7 kHz, 38%) ja esta configurado no CCP1 (RC2).
 * As entradas IN1/IN2 do L293D definem o sentido de rotacao.
 * SetDCPWM1(272) ativa o motor; SetDCPWM1(0) para o sinal PWM.
 */
void motor_liga(void) {
    if (direcao == SUBINDO) { MOTOR_IN1 = 1; MOTOR_IN2 = 0; }
    else                    { MOTOR_IN1 = 0; MOTOR_IN2 = 1; }
    SetDCPWM1(PWM_DUTY);        /* 38% ciclo ativo                          */
}

void motor_desliga(void) {
    SetDCPWM1(0);               /* zerando o duty cycle para o PWM          */
    MOTOR_IN1 = 0;
    MOTOR_IN2 = 0;
}

/* -- PORTA ---------------------------------------------------- */
void porta_abre(void)  { PORTA_LED = 1; }
void porta_fecha(void) { PORTA_LED = 0; }

/* -- MAQUINA DE ESTADOS --------------------------------------- */
/*
 * Converte ADC (0-1023) em temperatura inteira (0-50  grausC):
 *   temp = adc_raw * 50 / 1023
 * Para 1 decimal: temp_10 = adc_raw * 500 / 1023 (decimos de grau)
 *
 * Todas as temporizacoes usam ms_tick (incrementado pela ISR do Timer0).
 * Tecnica: registra o instante de inicio (t_xxx = ms_tick) e compara
 * com (ms_tick - t_xxx) >= LIMIAR -- nao bloqueia o loop principal.
 */
void maquina_estados(void) {
    int temp = (int)((unsigned long)adc_raw * 50UL / 1023UL);

    switch (estado) {

        /* -- IDLE: motor parado, aguarda botao ou timeout ----------- */
        case IDLE:
            motor_desliga();

            if (temp > TEMP_MAX) { estado = SUPERAQUECIDO; break; }

            andar_destino = proximo_fila();
            if (andar_destino != 0) {
                definir_direcao();
                motor_liga();
                t_viagem  = ms_tick;
                t_inativo = ms_tick;
                estado    = MOVENDO;
                break;
            }
            /* 2,3 s sem chamada e nao esta no 3o andar -> retorna */
            if ((ms_tick - t_inativo) >= TEMPO_INATIVO
                    && andar_atual != ANDAR_HOME) {
                andar_destino = ANDAR_HOME;
                definir_direcao();
                motor_liga();
                t_viagem  = ms_tick;
                t_inativo = ms_tick;
                estado    = RETORNANDO;
            }
            break;

        /* -- MOVENDO / RETORNANDO: avanca 1 andar a cada TEMPO_POR_ANDAR */
        case MOVENDO:
        case RETORNANDO:
            if (temp > TEMP_MAX) { motor_desliga(); estado = SUPERAQUECIDO; break; }

            if ((ms_tick - t_viagem) >= TEMPO_POR_ANDAR) {
                t_viagem = ms_tick;
                if (direcao == SUBINDO)  andar_atual++;
                if (direcao == DESCENDO) andar_atual--;

                if (andar_atual == andar_destino) {
                    fila[andar_destino] = 0;    /* remove da fila           */
                    motor_desliga();
                    porta_abre();
                    t_porta = ms_tick;
                    estado  = PORTA_ABERTA;
                }
            }
            break;

        /* -- PORTA_ABERTA: aguarda 1 s e fecha ---------------------- */
        case PORTA_ABERTA:
            if ((ms_tick - t_porta) >= TEMPO_PORTA) {
                porta_fecha();
                t_inativo = ms_tick;
                estado    = IDLE;
            }
            break;

        /* -- SUPERAQUECIDO: temp > 31  grausC -- motor parado ate resfriar  */
        case SUPERAQUECIDO:
            motor_desliga();
            direcao = PARADO;
            if (temp <= TEMP_MAX) {             /* temperatura normalizada  */
                t_inativo = ms_tick;
                if (andar_destino != 0 && andar_destino != andar_atual) {
                    definir_direcao();          /* retoma viagem pendente   */
                    motor_liga();
                    t_viagem = ms_tick;
                    estado   = MOVENDO;
                } else {
                    estado = IDLE;
                }
            }
            break;
    }
}

/* -- ATUALIZAR LCD -------------------------------------------- */
/*
 * Linha 1 (16 chars): "Andar: X  [^]   "
 *   ^/v = char customizado CGRAM  |  "STOP" se SUPERAQUECIDO
 *
 * Linha 2 (16 chars): "Tmp: XX.X C     "
 *   temperatura em decimos de grau (sem float, usando inteiros)
 *   Ex.: adc=640 -> temp_10 = 640*500/1023 = 312 -> "31.2 C"
 */
void atualizar_lcd(void) {
    char buf[18];
    unsigned int temp_10;
    unsigned long elapsed;
    static unsigned long t_anim = 0;
    static unsigned char frame  = 0;

    /* troca frame a cada 350 ms -- cria animacao de setas e pontos          */
    if ((ms_tick - t_anim) >= 350UL) {
        t_anim = ms_tick;
        frame  = (unsigned char)((frame + 1) & 0x03); /* cicla 0-1-2-3     */
    }

    temp_10 = (unsigned int)((unsigned long)adc_raw * 500UL / 1023UL);

    /* -- linha 1: "Andar: X" + seta animada ---------------------- */
    lcd_posicao(1, 1);
    sprintf(buf, "Andar: %d  ", andar_atual);
    imprime_buffer_lcd(buf, 10);               /* 10 chars fixos             */

    if (estado == SUPERAQUECIDO) {
        imprime_string_lcd("STOP  ");          /* 6 chars -> total 16         */
    } else if (direcao == SUBINDO) {
        /* alterna char 0 (UP_A) e char 1 (UP_B) = seta pulsando para cima  */
        lcd_escreve_dado((unsigned char)(frame & 1));
        imprime_string_lcd("     ");           /* 5 chars -> total 16         */
    } else if (direcao == DESCENDO) {
        /* alterna char 2 (DOWN_A) e char 3 (DOWN_B) = seta pulsando baixo  */
        lcd_escreve_dado((unsigned char)(2 + (frame & 1)));
        imprime_string_lcd("     ");
    } else {
        imprime_string_lcd("      ");          /* parado: 6 espacos          */
    }

    /* -- linha 2: mensagem de estado ----------------------------- */
    lcd_posicao(2, 1);

    /* prioridade: exibe "Chamado: Ax" por TEMPO_MSG_CHAMADO ms            */
    if (msg_chamado[0] != '\0' && (ms_tick - t_chamado) < TEMPO_MSG_CHAMADO) {
        imprime_buffer_lcd(msg_chamado, 16);
        return;
    }

    switch (estado) {
        case SUPERAQUECIDO:
            imprime_string_lcd("Temp elevada!   ");   /* 16 chars           */
            break;

        case PORTA_ABERTA:
            elapsed = ms_tick - t_porta;
            if      (elapsed < 1000UL) imprime_string_lcd("Abrindo porta   ");
            else if (elapsed < 2000UL) imprime_string_lcd("Porta aberta!   ");
            else                       imprime_string_lcd("Fechando porta  ");
            break;

        case MOVENDO:
        case RETORNANDO:
            if (direcao == SUBINDO) {
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
            sprintf(buf, "Tmp: %u.%uC       ", temp_10 / 10, temp_10 % 10);
            imprime_buffer_lcd(buf, 16);
            break;
    }
}
