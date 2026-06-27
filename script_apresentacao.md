# Script de Apresentação — Central para Elevador
**ELE1012 — Microcontroladores | UTFPR Medianeira | Prof. Miyadaira**

---

## SLIDE 1 — CAPA
**[WILLIAN abre a apresentação]**

> "Bom dia, professor. Nosso grupo vai apresentar o trabalho de Central para Elevador com 6 andares usando o PIC18F4550. Sou o Willian, RA 1869027. Meus colegas são a Karoline, RA 2578000; a Ana Caroline, RA 2486024; e o Erick, RA 2301105. Vou começar apresentando a visão geral do projeto."

---

## SLIDE 2 — VISÃO GERAL DO PROJETO
**[WILLIAN]**

> "O trabalho tem seis requisitos principais definidos pelo professor.
>
> Primeiro, o elevador tem 6 andares, cada um com um botão de chamada conectado ao PORTB do PIC.
>
> Segundo, um display LCD 2×16 mostra em tempo real o andar atual, a temperatura medida e o sentido de deslocamento — para isso criamos os símbolos de seta para cima e para baixo diretamente na memória CGRAM do display.
>
> Terceiro, a temperatura é simulada por um potenciômetro no pino RA0. Se passar de 31 graus Celsius, o elevador para imediatamente.
>
> Quarto, se o elevador ficar 2,3 segundos sem nenhuma chamada, ele retorna automaticamente ao terceiro andar.
>
> Quinto, ao chegar no andar solicitado, a porta passa por 4 fases de 1 segundo cada: Abrindo porta, Porta aberta, Fechando porta e Porta fechada — 4 segundos no total. O LED de porta apaga ao entrar na fase Fechando, aos 2 segundos.
>
> E sexto, o sinal de controle do motor é um PWM de aproximadamente 7 quilohertz com ciclo ativo de 38%.
>
> Toda a simulação foi feita no PICSimLab com o MPLAB e compilador C18."

---

## SLIDE 3 — DIVISÃO DO TRABALHO
**[WILLIAN]**

> "O conteúdo foi dividido assim: eu apresento a parte de hardware, pinagem e PWM. A Karoline apresenta o Timer0 e o ADC. A Ana Caroline apresenta o LCD e os botões. E o Erick apresenta a máquina de estados e o display. Vamos começar."

---

## SLIDE 4 — HARDWARE: PINAGEM
**[WILLIAN]**

> "Aqui está o mapeamento de todos os pinos utilizados.
>
> O PORTB recebe os seis botões nos pinos RB0 a RB5. Usamos o pull-up interno do PIC, então cada botão é ativo em nível baixo — quando pressionado, o pino vai a zero.
>
> O pino RA0, que é o canal AN0 do ADC, recebe o potenciômetro que simula a temperatura.
>
> O pino RC2 é o CCP1, que é a saída do PWM para controlar o motor.
>
> Em PORTD: RD0 e RD1 são as entradas IN1 e IN2 de um driver de motor tipo L293D, que definem a direção de rotação. RD2 é o LED que indica se a porta está aberta ou fechada. E os pinos RD4 a RD7 são o barramento de dados de 4 bits do LCD.
>
> No PORTE, os pinos RE0, RE1 e RE2 são os sinais de controle do LCD — RW, Enable e RS — gerenciados internamente pela biblioteca do professor Miyadaira."

---

## SLIDE 5 — CONFIG BITS + INIT_HARDWARE
**[WILLIAN]**

> "Para o oscillador, usamos FOSC igual a HS, que é o modo de cristal de alta velocidade, com um cristal externo de 20 MHz. O CPUDIV igual a OSC1_PLL2 garante que o PIC corra na frequência de Fosc sem acionar o PLL, que só seria necessário para USB. O watchdog está desabilitado para não reiniciar o PIC durante a simulação.
>
> Na função init_hardware, a primeira linha faz ADCON1 igual a 0x0F. Isso coloca PCFG em 1111, que é o código para desabilitar todos os canais analógicos. Fazemos isso porque o reset padrão do PIC pode deixar alguns pinos do PORTB como analógicos, o que impediria a leitura dos botões. O canal AN0 é reabilitado depois, na função init_adc.
>
> Em seguida, configuramos TRISB como 0xFF para que todos os bits do PORTB sejam entradas. Colocamos RBPU igual a zero para habilitar os pull-ups internos — é o bit 7 do INTCON2, e zero significa habilitado.
>
> Por fim, configuramos RD0, RD1 e RD2 como saídas, zeramos LATD, e configuramos RC2 como saída para o PWM e RA0 como entrada para o ADC."

---

## SLIDE 6 — PWM: CÁLCULO E CONFIGURAÇÃO
**[WILLIAN]**

> "Agora o cálculo do PWM. O módulo CCP1 do PIC usa o Timer2 como base de tempo.
>
> A fórmula da frequência é Fosc dividido por quatro vezes o prescaler vezes PR2 mais 1. Com Fosc de 20 MHz, prescaler de 1:4 e PR2 igual a 178, obtemos: 20 milhões dividido por 4 vezes 4 vezes 179, que dá 6.983 Hz — aproximadamente 7 quilohertz. Verificado.
>
> Para o ciclo ativo de 38%, o hardware usa um valor de 10 bits. Esse valor é calculado como a fração desejada vezes 4 vezes PR2 mais 1: 0,38 vezes 716 = 272.
>
> Os 10 bits são divididos: os 8 bits mais significativos vão para o registrador CCPR1L, que é 272 dividido por 4 = 68. Os 2 bits menos significativos vão para os bits DC1B1 e DC1B0 do CCP1CON, que é 272 módulo 4 = 0.
>
> O duty cycle real é 272 dividido por 716 = 37,99%, praticamente 38%.
>
> No código, usamos a biblioteca pwm.h do C18: OpenTimer2 com prescaler 1:4, OpenPWM1 com PR2 igual a 178, e SetDCPWM1 de zero para iniciar desligado. Quando o motor precisa ligar, chamamos SetDCPWM1 de 272. Para desligar, SetDCPWM1 de zero. O sinal PWM continua existindo no pino, mas com duty zero não há corrente no motor."

---

## SLIDE 7 — TIMER0: BASE DE TEMPO DE 1 ms
**[KAROLINE]**

> "Boa tarde. Eu sou a Karoline, e vou apresentar o Timer0 e o ADC.
>
> O Timer0 é o coração do sistema de temporização do projeto. Precisamos medir 2.300 milissegundos de inatividade, 1.000 milissegundos de porta aberta, e 3.000 milissegundos de viagem entre andares — tudo ao mesmo tempo, sem bloquear o loop principal. Para isso, configuramos o Timer0 para gerar uma interrupção exatamente a cada 1 milissegundo.
>
> O cálculo é o seguinte: com Fosc de 20 MHz, o ciclo de máquina é 4 dividido por 20 MHz = 200 nanossegundos. Com o prescaler em 1:8, cada incremento do Timer0 leva 1,6 microsegundos. Para completar 1 milissegundo, são necessários 625 incrementos.
>
> Como o Timer0 está em modo de 16 bits, ele conta de 0 a 65.535. Para fazer ele transbordar exatamente em 625 contagens, recarregamos ele com 65.536 menos 625, que é 64.911, ou 0xFD8F em hexadecimal.
>
> Na ISR, incrementamos a variável global ms_tick, recarregamos TMR0H com 0xFD e TMR0L com 0x8F, e limpamos a flag TMR0IF. Essa interrupção dura microsegundos e o sistema principal continua rodando normalmente.
>
> O vetor de interrupção de alta prioridade fica no endereço 0x0008. Usamos o padrão do C18 com pragma code e goto para direcionar o fluxo para a ISR."

---

## SLIDE 8 — ADC: TEMPERATURA
**[KAROLINE]**

> "Agora o ADC. O potenciômetro conectado em RA0 simula a temperatura. O ADC do PIC tem resolução de 10 bits, então retorna valores de 0 a 1023 correspondentes a 0 a 5 volts.
>
> Para configurar o ADC, primeiro definimos a tensão de referência como VDD e VSS — os bits VCFG em zero. Depois, em ADCON1, colocamos PCFG em 1110, que é o código para deixar apenas AN0 como analógico e todos os outros canais como digitais.
>
> Uma particularidade importante do nosso projeto: o PICSimLab simula o PIC18F452, não o PIC18F4550. No PIC18F452, o registrador ADCON2 não existe. O bit ADFM, que justifica o resultado à direita, fica no bit 7 do próprio ADCON1. Por isso configuramos ADCON1 como 0x8E: bit 7 igual a 1 para resultado justificado à direita, e PCFG igual a 1110 para deixar somente AN0 analógico.
>
> Para disparar a conversão, usamos ADCON0 OR 0x04, pois no PIC18F452 o bit GO fica na posição 2, não na posição 1 como no PIC18F4550. E em vez de aguardar a flag ADIF — que não funciona corretamente no simulador — usamos um delay de Delay10KTCYx de 1, equivalente a aproximadamente 2 milissegundos, tempo suficiente para a conversão completar. O resultado é lido combinando ADRESH deslocado 8 bits à esquerda com ADRESL, retornando um valor de 10 bits entre 0 e 1023.
>
> O mapeamento de temperatura: multiplicamos o valor do ADC por 50 e dividimos por 1023 para obter graus Celsius inteiros. Para exibir uma casa decimal, multiplicamos por 500 e dividimos por 1023 — isso nos dá décimos de grau. Usamos unsigned long nas multiplicações para evitar overflow de 16 bits. Não usamos float em nenhum momento, porque o C18 LITE tem limitações com ponto flutuante."

---

## SLIDE 9 — LCD: BIBLIOTECA MIYADAIRA
**[ANA CAROLINE]**

> "Boa tarde. Eu sou a Ana Caroline, e vou apresentar o LCD e os botões.
>
> O display LCD 2×16 é controlado pela biblioteca_lcd_2x16.h do professor Miyadaira, que usa 4 bits de dados nos pinos RD4 a RD7, e os sinais de controle RS, RW e Enable nos pinos RE2, RE0 e RE1 respectivamente.
>
> As principais funções que usamos são: lcd_inicia, que inicializa o display; lcd_posicao, que move o cursor para uma linha e coluna específicas — numeradas a partir de 1; lcd_escreve_dado, que escreve um único byte no display; imprime_string_lcd, que envia uma string armazenada na memória de programa; e imprime_buffer_lcd, que envia um número específico de bytes de um buffer na RAM.
>
> Para inicializar, chamamos lcd_inicia com três parâmetros: 0x28, que configura modo de 4 bits, 2 linhas e matriz 8×5; 0x0F, que liga o display e ativa o cursor piscante; e 0x06, que faz o cursor se mover para a direita após cada caractere.
>
> Uma função que usamos muito é lcd_envia_controle, que envia comandos diretos para o controlador do display, incluindo comandos de escrita na memória CGRAM — que veremos no próximo slide."

---

## SLIDE 10 — CGRAM: CARACTERES ↑ E ↓
**[ANA CAROLINE]**

> "O controlador HD44780 do LCD tem uma memória especial chamada CGRAM, que permite criar até 8 caracteres personalizados de 5 por 8 pixels. Nós criamos dois: a seta para cima no slot 0 e a seta para baixo no slot 1.
>
> Cada caractere é definido por 8 bytes, onde cada byte representa uma linha de pixels — os 5 bits menos significativos definem os pixels ativos. O bit mais significativo de cada byte corresponde à coluna da esquerda.
>
> Para a seta para cima, vemos na grade visual que a ponta está no topo e o cabo vai descendo. O byte 0x04 ativa só o pixel central na primeira linha, 0x0E ativa os três centrais na segunda, 0x1F ativa todos os cinco na terceira, e depois só o pixel central nas linhas seguintes.
>
> Para gravar na CGRAM, enviamos o comando 0x40 com lcd_envia_controle — isso aponta o cursor para a primeira posição da CGRAM, que é o caractere 0. Depois enviamos os 8 bytes com lcd_escreve_dado. Para o caractere 1, o endereço é 0x48.
>
> Após gravar, chamamos lcd_posicao para voltar o cursor ao DDRAM — a memória de dados do display.
>
> Para exibir o símbolo no display, usamos lcd_escreve_dado com zero para mostrar a seta para cima, e com 1 para mostrar a seta para baixo."

---

## SLIDE 11 — BOTÕES: DEBOUNCE
**[ANA CAROLINE]**

> "Os 6 botões estão conectados nos pinos RB0 a RB5. O pull-up interno está habilitado, então o nível lógico normal é HIGH. Quando o botão é pressionado, o pino vai a LOW — por isso usamos lógica invertida: o bit igual a zero significa que o botão está ativo.
>
> O problema dos botões mecânicos é o bouncing. Quando pressionamos um botão, o contato mecânico oscila por cerca de 5 a 20 milissegundos antes de estabilizar. Sem tratamento, uma única pressão poderia ser lida como vários acionamentos.
>
> Nossa solução usa debounce por tempo. Mantemos um array estático t_deb com 7 posições, onde guardamos o timestamp em milissegundos do último acionamento aceito para cada andar.
>
> A cada chamada da função ler_botoes, lemos o PORTB inteiro de uma vez para ter um snapshot consistente. Para cada andar de 1 a 6, verificamos se o bit correspondente está em zero. Se estiver, verificamos se já passaram pelo menos 20 milissegundos desde o último acionamento aceito. Se sim, atualizamos o timestamp, e se o andar pressionado não for o andar atual, adicionamos o andar à fila e reiniciamos o contador de inatividade."

---

## SLIDE 12 — MÁQUINA DE ESTADOS: VISÃO GERAL
**[ERICK]**

> "Boa tarde. Eu sou o Erick, e vou apresentar a máquina de estados e o display.
>
> Toda a lógica do elevador é controlada por uma máquina de estados com 5 estados. O loop principal executa quatro funções repetidamente: ler o ADC, ler os botões, executar o estado atual, e atualizar o LCD.
>
> No estado IDLE, o motor está parado e o sistema aguarda uma chamada ou o timeout de 2,3 segundos. No estado MOVENDO, o motor está ativo e o elevador avança um andar a cada 3 segundos simulados — isso é controlado pelo ms_tick. O estado RETORNANDO funciona exatamente igual ao MOVENDO, mas o destino é sempre o terceiro andar.
>
> No estado PORTA_ABERTA, o motor está parado e o LED de porta está aceso. O sistema executa 4 fases de 1 segundo: Abrindo porta, Porta aberta, Fechando porta e Porta fechada — 4 segundos totais, controlados por TEMPO_PORTA. O LED apaga aos 2 segundos, início da fase Fechando. No estado SUPERAQUECIDO, o motor para imediatamente e o sistema aguarda a temperatura cair abaixo de 31 graus.
>
> As transições são verificadas a cada iteração do loop. A temperatura é sempre verificada nos estados de movimento — se subir, vai para SUPERAQUECIDO instantaneamente."

---

## SLIDE 13 — ESTADOS IDLE E MOVENDO
**[ERICK]**

> "Vamos ver o código dos estados IDLE e MOVENDO em detalhe.
>
> No IDLE: primeiro desligamos o motor para garantir que está parado. Depois verificamos a temperatura — se ultrapassou 31 graus, vai direto para SUPERAQUECIDO.
>
> Em seguida, consultamos a fila chamando proximo_fila. Essa função implementa o algoritmo SCAN: se estamos subindo, procura o próximo andar acima do atual; se não houver, inverte e procura abaixo. Isso é o mesmo algoritmo usado em elevadores reais, que evita ficar subindo e descendo sem critério.
>
> Se houver um destino, definimos a direção, ligamos o motor, registramos o tempo de início da viagem em t_viagem, e mudamos para MOVENDO.
>
> Se não houver chamadas e já passaram 2.300 milissegundos desde a última atividade — verificado pela subtração ms_tick menos t_inativo — e o elevador não estiver já no terceiro andar, mudamos para RETORNANDO com destino igual a ANDAR_HOME.
>
> No MOVENDO: verificamos temperatura a cada iteração. Verificamos se passou o tempo de viagem de 3 segundos. Se sim, avançamos um andar na direção correta e verificamos se chegamos ao destino. Se chegamos, removemos da fila, desligamos o motor, abrimos a porta e mudamos para PORTA_ABERTA."

---

## SLIDE 14 — PORTA_ABERTA E SUPERAQUECIDO
**[ERICK]**

> "No estado PORTA_ABERTA, a lógica acompanha o tempo decorrido desde que a porta abriu usando a variável t_porta. Até 1 segundo exibe Abrindo porta; de 1 a 2 segundos exibe Porta aberta; de 2 a 3 segundos exibe Fechando porta — e é exatamente aos 2 segundos que o LED de porta apaga, simulando o fechamento. De 3 a 4 segundos exibe Porta fechada. Após os 4 segundos completos, reiniciamos o contador de inatividade e voltamos para IDLE.
>
> No estado SUPERAQUECIDO, desligamos o motor e colocamos a direção como PARADO a cada iteração — isso garante que o LCD vai mostrar STOP em vez de uma seta. Depois verificamos se a temperatura voltou para 31 graus ou abaixo.
>
> Se a temperatura normalizou e havia um destino pendente que não é o andar atual, o elevador retoma a viagem imediatamente — define direção, liga motor e vai para MOVENDO. Caso contrário, volta para IDLE normalmente.
>
> As funções motor_liga e motor_desliga são simples: motor_liga verifica a direção atual e seta IN1 e IN2 para o lado correto no L293D, depois chama SetDCPWM1 com 272 para os 38% de duty. motor_desliga chama SetDCPWM1 com zero e coloca IN1 e IN2 em zero. porta_abre e porta_fecha simplesmente ligam e desligam o LED no RD2."

---

## SLIDE 15 — ATUALIZAR LCD
**[ERICK]**

> "A função atualizar_lcd é chamada a cada iteração do loop principal. Ela atualiza as duas linhas do display.
>
> Para a temperatura com uma casa decimal sem usar float: multiplicamos o valor bruto do ADC por 500 e dividimos por 1023, obtendo décimos de grau em um unsigned int. Dividir esse valor por 10 dá a parte inteira, e o módulo de 10 dá o dígito decimal.
>
> Na linha 1: posicionamos o cursor em linha 1 coluna 1. Usamos sprintf para montar a string 'Andar: X  ' com o andar atual — exatamente 10 caracteres — e enviamos com imprime_buffer_lcd.
>
> Depois vem o indicador de direção. Se o estado é SUPERAQUECIDO, enviamos a string 'STOP  '. Se a direção é SUBINDO, chamamos lcd_escreve_dado com zero para mostrar o caractere customizado de seta para cima. Se é DESCENDO, lcd_escreve_dado com 1 para a seta para baixo. Parado: seis espaços para limpar essa área. Em todos os casos completamos os 16 caracteres da linha.
>
> Na linha 2: usamos sprintf com a formatação %u ponto %u C para exibir a temperatura. A divisão inteira e o módulo geram a parte inteira e o decimal respectivamente. Enviamos 16 caracteres fixos com imprime_buffer_lcd, incluindo espaços de preenchimento para apagar caracteres antigos.
>
> Como podemos ver no mock do LCD ao lado, o display fica assim: 'Andar: 3  ↑     ' e 'Tmp: 28.5C      '. Em situação de superaquecimento, aparece 'Andar: 2  STOP  '."

---

## SLIDE 16 — CONCLUSÃO
**[WILLIAN fecha]**

> "Então, resumindo o que foi implementado neste trabalho:
>
> O Timer0 gera uma base de tempo precisa de 1 milissegundo usando interrupção, o que permite todas as contagens simultâneas sem bloquear o programa.
>
> O PWM no CCP1 gera aproximadamente 7 quilohertz com ciclo ativo de 38%, calculado analiticamente a partir da fórmula do datasheet.
>
> O ADC lê o potenciômetro e mapeia para 0 a 50 graus usando aritmética inteira, sem ponto flutuante.
>
> O LCD usa a biblioteca do professor com 4 bits de dados e exibe símbolos personalizados criados na CGRAM.
>
> A máquina de estados com 5 estados gerencia toda a lógica do elevador de forma não-bloqueante.
>
> E o debounce por tempo de 20 milissegundos garante leitura confiável dos botões.
>
> O projeto está disponível para demonstração no PICSimLab. Agradecemos a atenção e ficamos à disposição para perguntas."

---

## DICAS PARA A APRESENTAÇÃO

**Tempo estimado por parte:**
| Integrante | Slides | Tempo estimado |
|---|---|---|
| Willian | 1, 2, 3, 4, 5, 6 | ~8 minutos |
| Karoline | 7, 8 | ~6 minutos |
| Ana Caroline | 9, 10, 11 | ~6 minutos |
| Erick | 12, 13, 14, 15, 16 | ~7 minutos |
| **Total** | **16 slides** | **~27 minutos** |

**Frases-chave para saber de cor:**
- "ms_tick incrementa a cada 1 ms na ISR do Timer0, sem bloquear o loop"
- "PR2 = 178, prescaler 1:4, Fosc 20 MHz → 6.983 Hz ≈ 7 kHz"
- "Duty 38% = valor 10 bits 272, calculado como 0,38 × 4 × (PR2+1)"
- "PCFG = 1110: somente AN0 analógico"
- "CGRAM: lcd_envia_controle(0,0,0x40,45) aponta para o caractere 0"
- "Debounce: aceita botão só se passaram ≥ 20 ms desde o último"
- "proximo_fila() usa algoritmo SCAN: prioriza direção atual"
