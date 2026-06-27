# STATE.md — Microcontroladores ELE1012

**Atualizado:** 2026-06-26 (sessão 9 — AP2 substituída pelo trabalho; arquivos de apoio adicionados)

---

## Estado atual

### Trabalho Elevador — `microcontroladores/elevador/elevador.c`
- **BUILD SUCCEEDED** — `noboro.hex` pronto em `OneDrive/Documentos/Micro-controladores/`
- **FIX-017:** LCD throttle 100ms — elimina piscar preto causado por `lcd_posicao()` em loop livre
- **FIX-018:** Linha 1 exibe `"PARADO"` no estado IDLE (era 6 espaços em branco)
- **FIX-019:** `direcao = PARADO` ao chegar no destino — seta sumia apenas ao entrar no IDLE; agora some imediatamente ao parar (e fix do retorno ao 3° andar sempre mostrando ↑)
- **FIX-020:** `TEMPO_PORTA` 3000→4000ms, 4 fases de 1s: "Abrindo porta" / "Porta aberta!" / "Fechando porta" / "Porta fechada"; LED apaga em 2000ms (início do fechamento)
- **FIX-021:** Botão no andar atual + estado IDLE → abre porta diretamente (antes era ignorado)
- LCD funcional no PICSimLab: `Andar: 3  PARADO` / `Andar: 3 ↑` / `Subindo....` / `Abrindo porta` / `Porta aberta!` / `Fechando porta` / `Porta fechada` / `Chamado: A2`
- Setas ↑↓ animadas (4 chars CGRAM, alternância a cada 350ms)
- Botões: RB0=A1, RB1=A2, ..., RB5=A6 — clicar nos botões B0–B5 do bloco LED2/PORTB no PICSimLab
- Temperatura: trimpot ANAL0 em RA0/AN0 — **habilitar DIP switch AN0 (posição 9, superior)** no PICSimLab
- Trimpot: ANAL0 = **um dos 2 círculos brancos no CENTRO da placa** (NÃO o azul ao lado do LCD — esse é contraste do LCD, sem ligação ao PIC)
- Comentarios do .c convertidos de UTF-8 para Windows-1252 (ANSI) -- legivel no MPLAB v8.92
- Bug ADC corrigido: ADCON0=0x05 lia AN0 no PIC18F452 (nao AN1); revertido para ADCON0=0x01 (AN0, compativel ambos)
- **ADC PICSimLab fix (2026-06-25):** `init_adc()` simplificado para `ADCON1 = 0x8E` (ADFM=1 em bit7 + PCFG=1110); `ler_adc()` usa `ADCON0 |= 0x04` (GO=bit2) + `Delay10KTCYx(1)` — elimina resultado left-justified e timeout falso
- DIP corretos: superior pos.9 (AN0) ON; inferior pos.1 (LCD) ON; resto OFF
- TEMPO_PORTA=3000ms (1s abrindo + 1s aberta + 1s fechando; enunciado diz 1s)
- Pinagem conforme script: motor RD0/RD1, LED porta RD2, LCD RD4-RD7, ADC RA0
- **Apresentação: 02/07/2026** — AP2 foi substituída pelo trabalho do elevador (sem prova escrita)
- **GitHub:** `github.com/Eng-Willian-DS/elevador-pic18` (público) — elevador.c + PPT + script + docs/STATE + docs/FIXES + README detalhado
- `gera_ppt.py` no .gitignore — existe localmente, não aparece no repo
- Git local: `D:\Projetos\UTFPR\microcontroladores\elevador\` (branch master → remote main)

### Materiais de aula
- Todos os 12 materiais de aula transcritos para markdown
- Manual do kit PICGenios documentado com mapeamento completo de pinos
- Planejamento de aula + questões da AP1 documentados
- Mural do Classroom (códigos do professor + atividades) documentados
- **Atividade 05:** código corrigido, comentado e verificado no MPLAB (build OK)
- **Atividade 05:** relatório HTML de revisão técnica gerado (10 erros + 3 erros de build)
- **Atividade 06:** código limpo e comentado gerado
- **Atividade 07:** código completo gerado (PWM CCP1 via UART, 0–100%)

## Atividades — soluções produzidas

### Atividade 05 (entrega 08/jun)
- USART 19200 bps, ISR RX, `getcUSART()` / `OpenUSART()`
- LCD linha 1: exibe nome recebido caractere a caractere
- LCD linha 2: estado do buzzer (`Buzzer: LIGADO` / `Buzzer: DESLIG.`)
- Comandos: `L`/`l` liga RC1, `D`/`d` desliga RC1
- Bug do código do colega corrigido: RS era RE0 (R/W), correto é RE2; Buzzer era RC2 (cooler), correto é RC1
- Testado via VSPE (par COM1↔COM2) + Hercules (19200, 8N1)

### Atividade 06 (entrega 08/jun)
- Motor de passo em RD0–RD3, sequência wave drive: 0x01→0x02→0x04→0x08
- Velocidade ajustável via ADC AN0 (trimpot RA0): 15ms–500ms por passo
- `atraso_ms()` com `Delay1KTCYx(5)` = 1ms @ 20MHz
- LCD linha 1: "Motor de Passo", linha 2: "Vel: XXXms/step"
- Chave DIP: ANAL0 habilitado

### Atividade 07 (entrega 12/jun)
- PWM CCP1 em RC2, Timer2 prescaler 1:4, PR2=200 (~6,2 kHz)
- Recebe número 0–100 via UART 19200bps + Enter → aplica como % do duty cycle
- Duty cycle: `valor_10bits = pct * 804 / 100` → CCPR1L + DC1B1:DC1B0
- LCD linha 1: "PWM Control" | linha 2: "Duty: XX%"
- ISR acumula dígitos, sinaliza ao loop principal com flag
- **Concluído**

## Arquivos da pasta

| Arquivo | Conteúdo |
|---|---|
| `00-planejamento-aula.md` | Cronograma 18 semanas, avaliações, bibliografia |
| `00-kit-picgenios.md` | Manual do kit — pinagem, DIP switches, exemplos |
| `00-classroom-mural.md` | Códigos do professor + questões AP1 + atividades |
| `01-introducao.md` | Intro MCUs, Von-Neumann/Harvard, CISC/RISC, MPLAB |
| `02-pic18f4550-hardware.md` | Pinagem, memória, SFRs, clock, tipos de dados |
| `03-portas-configuracao.md` | #pragma config, TRIS/PORT/LAT, pull-ups |
| `04-delays-exercicios-io.md` | delays.h, cálculo de atraso, pisca-pisca |
| `05-wdt-display7seg.md` | WDT, display 7-seg, tabela segmentos, varredura |
| `06-lcd-alfanumerico.md` | LCD 16x2, init 4/8 bits, biblioteca, CGRAM |
| `07-interrupcoes.md` | INT0/1/2, ISR, prioridades, exemplos |
| `08-eusart.md` | RS-232, baud rate, usart.h, controle serial |
| `08b-serial-virtual.md` | VSPE + Hercules para simulação |
| `09-timers.md` | Timer0/1/2/3, prescaler, atraso_ms |
| `10-ccp-pwm-capture-compare.md` | PWM, ECCP, Capture, Compare |
| `11-adc.md` | ADC 10 bits, ADCON, adc.h, conversão automática |
| `12-comparador-analogico.md` | Comparador analógico, CVREF |

## Calendário restante

| Data | Evento | Status |
|---|---|---|
| 08/06/2026 | Entrega Atividade 05 | Código pronto ✓ |
| 08/06/2026 | Entrega Atividade 06 | Código pronto ✓ |
| 12/06/2026 | Entrega Atividade 07 | Código pronto ✓ |
| 10/06/2026 | Aula: Propostas de projetos | — |
| 11/06/2026 | Aula: CCP PWM | — |
| 17–18/06/2026 | Aula: CCP Compare | — |
| ~~01/07/2026~~ | ~~Avaliação 02 (AP2)~~ | Substituída pelo trabalho do elevador |
| 02/07/2026 | **Apresentação do elevador** | Próxima entrega ⚠️ |
| 08/07/2026 | Substitutiva | — |

## Próximo passo

- **Apresentação do trabalho elevador — 02/07/2026**
- Revisar slides `Central_Elevador.pptx` e script `script_apresentacao.md`
- Testar elevador.c no PICSimLab antes da apresentação
