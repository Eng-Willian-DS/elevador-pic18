# Script — JULIANO BARBOSA PAIXÃO
**RA 2041952 | ELE1012 — Microcontroladores | UTFPR Medianeira**
**Slides: 4, 5, 6 | Tempo estimado: ~5 minutos**

---

## SLIDE 4 — HARDWARE: PINAGEM
> "Obrigado, Willian. Eu sou o Juliano, RA 2041952, e vou apresentar o hardware do projeto.
>
> Aqui está o mapeamento de todos os pinos utilizados.
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
> "Para o oscillador, usamos FOSC igual a HS, que é o modo de cristal de alta velocidade, com um cristal externo de 20 MHz. O CPUDIV igual a OSC1_PLL2 garante que o PIC corra na frequência de Fosc sem acionar o PLL, que só seria necessário para USB. O watchdog está desabilitado para não reiniciar o PIC durante a simulação.
>
> Na função init_hardware, a primeira linha faz ADCON1 igual a 0x0F. Isso coloca PCFG em 1111, que é o código para desabilitar todos os canais analógicos. Fazemos isso porque o reset padrão do PIC pode deixar alguns pinos do PORTB como analógicos, o que impediria a leitura dos botões. O canal AN0 é reabilitado depois, na função init_adc.
>
> Em seguida, configuramos TRISB como 0xFF para que todos os bits do PORTB sejam entradas. Colocamos RBPU igual a zero para habilitar os pull-ups internos — é o bit 7 do INTCON2, e zero significa habilitado.
>
> Por fim, configuramos RD0, RD1 e RD2 como saídas, zeramos LATD, e configuramos RC2 como saída para o PWM e RA0 como entrada para o ADC."

---

## SLIDE 6 — PWM: CÁLCULO E CONFIGURAÇÃO
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

## FRASES-CHAVE PARA DECORAR
- "ADCON1 = 0x0F desabilita todos os analógicos — PORTB fica 100% digital"
- "RBPU = 0 habilita pull-ups internos (bit 7 do INTCON2, zero = habilitado)"
- "PR2 = 178, prescaler 1:4, Fosc 20 MHz → 6.983 Hz ≈ 7 kHz"
- "Duty 38% = valor 10 bits 272, calculado como 0,38 × 4 × (PR2+1)"
- "CCPR1L = 272 >> 2 = 68 | DC1B = 272 & 3 = 0"
