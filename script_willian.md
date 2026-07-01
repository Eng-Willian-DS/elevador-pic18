# Script — WILLIAN DOUGLAS DOS SANTOS CLEMENTINO
**RA 1869027 | ELE1012 — Microcontroladores | UTFPR Medianeira**
**Slides: 1, 2, 3, 7, 16 | Tempo estimado: ~6 minutos**

---

## SLIDE 1 — CAPA
> "Bom dia, professor. Nosso grupo vai apresentar o trabalho de Central para Elevador com 6 andares usando o PIC18F4550. Sou o Willian, RA 1869027. Meus colegas são o Juliano, RA 2041952; a Karoline, RA 2578000; a Ana Caroline, RA 2486024; e o Erick, RA 2301105. Vou começar apresentando a visão geral do projeto."

---

## SLIDE 2 — VISÃO GERAL DO PROJETO
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
> "O conteúdo foi dividido em cinco partes. Eu apresento a visão geral, o Timer0 e a conclusão. O Juliano apresenta o hardware, a pinagem e o PWM. A Karoline apresenta o ADC e o LCD. A Ana Caroline apresenta a CGRAM, os botões e a visão geral da máquina de estados. E o Erick apresenta os detalhes dos estados e o display. Vamos começar."

---

## SLIDE 7 — TIMER0: BASE DE TEMPO DE 1 ms
> "Obrigado, Juliano. Vou apresentar agora o Timer0, que é o coração do sistema de temporização.
>
> Precisamos medir 2.300 milissegundos de inatividade, 1.000 milissegundos de porta aberta, e 3.000 milissegundos de viagem entre andares — tudo ao mesmo tempo, sem bloquear o loop principal. Para isso, configuramos o Timer0 para gerar uma interrupção exatamente a cada 1 milissegundo.
>
> O cálculo é o seguinte: com Fosc de 20 MHz, o ciclo de máquina é 4 dividido por 20 MHz = 200 nanossegundos. Com o prescaler em 1:8, cada incremento do Timer0 leva 1,6 microsegundos. Para completar 1 milissegundo, são necessários 625 incrementos.
>
> Como o Timer0 está em modo de 16 bits, ele conta de 0 a 65.535. Para fazer ele transbordar exatamente em 625 contagens, recarregamos ele com 65.536 menos 625, que é 64.911, ou 0xFD8F em hexadecimal.
>
> Na ISR, incrementamos a variável global ms_tick, recarregamos TMR0H com 0xFD e TMR0L com 0x8F, e limpamos a flag TMR0IF. Essa interrupção dura microsegundos e o sistema principal continua rodando normalmente.
>
> O vetor de interrupção de alta prioridade fica no endereço 0x0008. Usamos o padrão do C18 com pragma code e goto para direcionar o fluxo para a ISR."

---

## SLIDE 16 — CONCLUSÃO
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

## FRASES-CHAVE PARA DECORAR
- "ms_tick incrementa a cada 1 ms na ISR do Timer0, sem bloquear o loop"
- "65.536 − 625 = 64.911 = 0xFD8F — preload do Timer0 para 1 ms"
- "Prescaler 1:8, Fosc 20 MHz → tick de 1,6 µs → 625 ticks = 1 ms"
