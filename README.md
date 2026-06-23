# Elevador PIC18F4550

---

## Disciplina

| Campo | Detalhe |
|-------|---------|
| Disciplina | ELE1012 — Microprocessadores e Sistemas Microcontrolados |
| Professor | Alberto Noboru Miyadaira |
| Instituicao | UTFPR — Universidade Tecnologica Federal do Parana, Campus Medianeira |
| Periodo | 2026/1 |
| AP2 | 01/07/2026 |
| Apresentacao | 02/07/2026 |

---

## Objetivo do projeto

Desenvolver uma central de controle para elevador de 6 andares utilizando o microcontrolador PIC18F4550, atendendo aos seguintes requisitos definidos pelo professor:

1. **6 andares** com botao de chamada individual conectado ao PORTB (RB0-RB5), ativo em nivel baixo com pull-up interno
2. **Display LCD 2x16** exibindo em tempo real: andar atual, sentido de deslocamento (setas animadas na CGRAM) e temperatura medida
3. **Controle de temperatura** via potenciometro no pino RA0/AN0 — se ultrapassar 31 graus C, o elevador para imediatamente (estado STOP)
4. **Retorno automatico** ao 3o andar apos 2,3 segundos sem nenhuma chamada
5. **Ciclo de porta** ao chegar no andar solicitado: abre, permanece aberta por 1 segundo e fecha
6. **Sinal PWM** de aproximadamente 7 kHz com ciclo ativo de 38% para controle do motor via L293D

Toda a simulacao foi desenvolvida e validada no PICSimLab com MPLAB IDE v8.92 e compilador C18 v3.47.

---

## Grupo

| Nome | Matrícula |
|------|-----------|
| Willian Douglas dos Santos Clementino | 1869027 |
| Karoline Yang | 2578000 |
| Ana Caroline Baptista | 2486024 |
| Erick Villalva | 2301105 |

**AP2:** 01/07/2026 · **Apresentação:** 02/07/2026

---

## Hardware e ferramentas

| Item | Detalhe |
|------|---------|
| MCU | PIC18F4550 @ 20 MHz (cristal HS) |
| Kit | PICGenios v3.0 |
| IDE | MPLAB IDE v8.92 + compilador C18 v3.47 LITE |
| Simulador | PICSimLab v0.8.6 (placa McLab2 / PICGenios) |
| Biblioteca LCD | `C:\PIC18\biblioteca_lcd_2x16.h` (PIC18.rar do Classroom) |

---

## Estrutura de arquivos

```
elevador-pic18/
├── elevador.c              # Codigo principal PIC18F4550
├── Central_Elevador.pptx   # Apresentacao — 16 slides
├── script_apresentacao.md  # Script de fala completo (~27 min)
├── docs/
│   ├── STATE.md            # Estado atual do projeto + calendario
│   └── FIXES.md            # Historico completo de bugs corrigidos (14 fixes)
└── README.md               # Este arquivo
```

---

## Pinagem

| Pino | Funcao |
|------|--------|
| RA0 / AN0 | Trimpot ANAL0 — simula temperatura 0-50 graus C |
| RB0 - RB5 | Botoes andares 1-6 (ativo em LOW, pull-up interno) |
| RC2 / CCP1 | PWM motor (~7 kHz, 38% duty cycle) |
| RD0 | Motor IN1 — L293D (direcao subir) |
| RD1 | Motor IN2 — L293D (direcao descer) |
| RD2 | LED porta (1 = aberta, 0 = fechada) |
| RD4 - RD7 | LCD data bus (gerenciado pela biblioteca) |
| RE0 | LCD R/W |
| RE1 | LCD Enable |
| RE2 | LCD RS |

---

## Configuracao DIP Switches no PICSimLab

> **IMPORTANTE:** qualquer DIP errado causa mal funcionamento (conflito de pinos).

| DIP | Posicao | Label | Estado |
|-----|---------|-------|--------|
| Superior | 9 | AN0 | **ON** |
| Inferior | 1 | LCD | **ON** |
| Todos os outros | — | — | **OFF** |

Por que cada um importa:
- `AN0 ON` — conecta o trimpot ANAL0 ao pino RA0 (leitura de temperatura)
- `VENT OFF` — evita conflito com PWM do motor (ambos usam RC2)
- `LED1/PORTD OFF` — evita conflito com barramento LCD (RD4-RD7)
- `LED2/PORTB OFF` — evita conflito com leitura dos botoes (RB0-RB5)
- `TEMP OFF` — sensor LM35 em RE0 nao faz parte do projeto

---

## Como compilar

1. Baixar `PIC18.rar` do Classroom e extrair em `C:\PIC18\`
2. Abrir `elevador.c` no MPLAB IDE v8.92
3. Criar projeto para **PIC18F4550** apontando para o arquivo
4. `Project -> Build All` (Ctrl+F10)
5. O `.hex` e gerado na pasta de saida do projeto (ex: `noboro.hex`)

> **Encoding:** `elevador.c` esta salvo em **Windows-1252 (ANSI)**. Nao abrir no VS Code
> e salvar — vai converter para UTF-8 e os comentarios viram lixo no MPLAB.
> Editar somente pelo MPLAB ou por editor que suporte ANSI.

---

## Como usar no PICSimLab

1. Abrir PICSimLab v0.8.6 → selecionar placa **PICGenios** (McLab2)
2. Carregar o `.hex` compilado
3. Configurar DIP switches conforme tabela acima
4. **Botoes dos andares:** clicar B0-B5 no bloco LED2/PORTB
   - B0 = Andar 1, B1 = Andar 2, ..., B5 = Andar 6
5. **Temperatura:** usar scroll do mouse sobre o trimpot ANAL0 (ao lado do LCD)
   - Rolar para cima = mais quente · rolar para baixo = mais frio
   - Acima de ~60% do curso → temperatura passa de 31 graus C → aparece STOP

---

## Comportamento do LCD

| Situacao | Linha 1 | Linha 2 |
|----------|---------|---------|
| Andar acionado (1,5 s) | `Andar: X` | `Chamado: A3` |
| Subindo | `Andar: X` + seta animada | `Subindo....` |
| Descendo | `Andar: X` + seta animada | `Descendo....` |
| Abrindo porta | `Andar: X` | `Abrindo porta` |
| Porta aberta | `Andar: X` | `Porta aberta!` |
| Fechando porta | `Andar: X` | `Fechando porta` |
| Parado / normal | `Andar: X` | `Tmp: 0.0C` |
| Superaquecido | `Andar: X STOP` | `Temp elevada!` |

As setas animadas alternam entre 2 bitmaps CGRAM a cada 350 ms (efeito de pulsacao).
Os pontos em `Subindo....` ciclam de 1 a 4 pontos a cada 350 ms.

---

## Maquina de estados

```
PARADO   ---(botao != andar atual)---►  MOVENDO
MOVENDO  ---(chegou ao destino)------►  CHEGANDO
CHEGANDO ---(ciclo porta concluido)--►  PARADO
PARADO   ---(2,3 s sem chamada)------►  retorna ao andar 3

Qualquer estado ---(temp > 31 C)-----►  STOP
STOP            ---(temp <= 31 C)----►  PARADO
```

Temporizadores:
- `TEMPO_PORTA = 3000 ms` (1 s abrindo + 1 s aberta + 1 s fechando)
- `TEMPO_INATIVO = 2300 ms` (retorna ao andar 3 se parado sem chamada)
- `TEMPO_POR_ANDAR = 3000 ms` (simulacao: 3 s de deslocamento por andar)

---

## Detalhes tecnicos

### Timer0 — base de tempo (1 ms)
- 16 bits · clock interno Fosc/4 = 5 MHz · prescaler 1:8
- 1 tick = 1,6 us → preload = 65536 - 625 = **0xFD8F** para 1 ms
- ISR incrementa `ms_tick` a cada 1 ms — base de todos os temporizadores do sistema

### PWM — motor (CCP1 / RC2)
- Timer2 prescaler 1:4 · PR2 = 178
- **Fpwm** = 20e6 / (4 x 4 x 179) = 6.983 kHz ~ 7 kHz
- Duty 38%: valor 10 bits = 272 → CCPR1L = 68, DC1B = 0
- Subir: IN1=HIGH, IN2=LOW · Descer: IN1=LOW, IN2=HIGH · Parar: IN1=IN2=LOW

### ADC — temperatura (AN0 / RA0)
- `ADCON0 = 0x01` → canal AN0 · `PCFG = 1110` → so AN0 analogico
- Formula: `temp_C = (adc_val x 50) / 1023` → faixa 0-50 graus C
- **Nota PICSimLab:** o simulador usa PIC18F452 (sem ADCON2); `ler_adc()` tem timeout
  para nao travar — no hardware real (PIC18F4550) isso nao e necessario

### CGRAM — caracteres customizados
4 posicoes carregadas na inicializacao:

| Posicao | Conteudo | Uso |
|---------|----------|-----|
| 0 | Seta para cima frame A | animacao subindo |
| 1 | Seta para cima frame B | animacao subindo |
| 2 | Seta para baixo frame A | animacao descendo |
| 3 | Seta para baixo frame B | animacao descendo |

---

## Historico de correcoes

Ver `docs/FIXES.md` para o historico completo (14 correcoes).

Principais fixes desta versao:

| ID | Problema | Solucao |
|----|----------|---------|
| FIX-003 | ISR usada antes de ser declarada (erro C18) | Forward declaration adicionada no topo |
| FIX-004 | `ler_adc()` travava em loop infinito no PICSimLab | Timeout counter adicionado |
| FIX-007 | Botoes nunca detectados (teclado matricial conflitava com motor) | Leitura direta RB0-RB5 |
| FIX-011 | LCD so mostrava andar e temperatura, sem mensagens de estado | `atualizar_lcd()` reescrita com todos os estados |
| FIX-013 | Temperatura nao variava com trimpot (canal ADC errado no PIC18F452) | ADCON0=0x01 (AN0) compativel com ambos os PICs |
| FIX-014 | Comentarios apareciam como lixo no MPLAB v8.92 | Arquivo convertido de UTF-8 para Windows-1252 (ANSI) |

---

## Estado atual (2026-06-23)

- BUILD SUCCEEDED — sem erros, apenas 3 warnings inofensivos (string literals em ROM)
- PICSimLab funcional: LCD animado, botoes B0-B5, temperatura via trimpot ANAL0
- Apresentacao (`Central_Elevador.pptx`) pronta — 16 slides com codigo real
- Script de fala completo por slide em `script_apresentacao.md`
