# FIXES.md — ELE1012 Microcontroladores
_Append-only. Data | Bug | Causa raiz | Solução_

---

## 2026-06-25 — Trabalho Elevador: 3 fixes (sessão 8 — lógica porta + direção)

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-019 | Seta de direção permanecia acesa ao abrir porta; retorno ao 3° andar sempre exibia ↑ | `direcao` nunca resetada ao chegar no destino — `motor_desliga()` corta PWM mas não toca na variável | `direcao = PARADO` inserido no bloco de chegada antes de `porta_abre()` em `maquina_estados()` |
| 2 | FIX-020 | Ciclo de porta com só 3 mensagens; LED apagava ao sair do estado (fora da janela visual) | `TEMPO_PORTA=3000ms` com 3 fases; `porta_fecha()` chamada na transição PORTA_ABERTA→IDLE, não no momento correto | `TEMPO_PORTA=4000ms`; 4 fases de 1s; `porta_fecha()` chamada em `elapsed>=2000ms` (início da fase Fechando) |
| 3 | FIX-021 | Pressionar botão do andar atual ignorava o acionamento sem nenhuma resposta | `ler_botoes()` ignorava `andar == andar_atual` incondicionalmente | `else if (estado == IDLE)` abre porta diretamente: `porta_abre()` + `t_porta=ms_tick` + `estado=PORTA_ABERTA` |

**Resultado:** porta com 4 fases visuais completas · seta some ao parar · botão do andar atual funciona

---

## 2026-06-25 — Trabalho Elevador: 2 fixes (sessão 7 — LCD flicker + indicador parado)

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-017 | LCD piscando preto aleatoriamente em vários pontos do visor | `atualizar_lcd()` chamada a cada iteração do `while(1)` (milhares de vezes/s); cada `lcd_posicao()` envia comando ao HD44780 causando flash visível | Throttle de 100ms adicionado no topo da função: `static t_lcd`; retorna cedo se `ms_tick - t_lcd < 100UL` |
| 2 | FIX-018 | Linha 1 do LCD sem nenhuma indicação quando elevador está parado | Ramo `else` da direção mostrava 6 espaços em branco (`"      "`) em vez de texto | Substituído por `"PARADO"` (6 chars, encaixa exatamente no espaço da seta) |

**Resultado:** LCD estável sem flicker · display mostra `Andar: 3  PARADO` no estado IDLE

---

## 2026-06-25 — Trabalho Elevador: 2 fixes (sessão 6 — ADC PICSimLab definitivo)

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-015 | Temperatura lida sempre incorreta no PICSimLab (valores > 1023 ou constantes) | `ADCON2bits.ADFM = 1` não tem efeito: ADCON2 não existe no PIC18F452; ADFM fica em ADCON1 bit7; resultado saía left-justified | `ADCON1 = 0x8E` (bit7=ADFM=1 + bits3:0=PCFG=1110); eliminadas todas as escritas em ADCON2 |
| 2 | FIX-016 | Conversão ADC irregular/instável no PICSimLab mesmo com timeout | `ADCON0bits.GO = 1` seta bit1; no PIC18F452 GO fica em bit2 → conversão não disparava corretamente | `ADCON0 \|= 0x04` (bit2=GO do PIC18F452) + `Delay10KTCYx(1)` substituindo `while(ADIF)` |

**Resultado:** `init_adc()` reduzido a 2 linhas; `ler_adc()` reduzido a 3 linhas; correção derivada do termometro.c validado na mesma sessão.

---

## 2026-06-23 — Trabalho Elevador: 2 fixes (sessão 5 — ADC revert + ANSI encoding)

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-013 | Temperatura ainda não variava mesmo com DIP AN1 (posição 10) | PIC18F452 (PICSimLab) usa bits[5:3] em ADCON0; ADCON0=0x05 → CHS=000 → AN0, não AN1 | Revertido para ADCON0=0x01 (AN0, canal correto em ambos PIC18F452 e PIC18F4550); DIP AN0 (posição 9) |
| 2 | FIX-014 | Comentários em elevador.c aparecem como lixo no MPLAB v8.92 | Arquivo salvo em UTF-8; MPLAB v8.92 espera Windows-1252 (ANSI) | PowerShell: `[System.IO.File]::ReadAllText(UTF8)` → substituições Unicode→ASCII → `WriteAllText(CP1252)`; 0 bytes não-ASCII restantes |

**Resultado:** BUILD SUCCEEDED · comentários legíveis no MPLAB · trimpot ANAL0 (ao lado do LCD) controla temperatura via AN0

---

## 2026-06-22 — Trabalho Elevador: 1 fix (sessão 4 — ADC canal correto)

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-012 | Temperatura sempre 0.0C no PICSimLab mesmo com DIP AN0 habilitado | Trimpot abaixo do RB0/INT é ANAL1 (RA1/AN1), não ANAL0 (RA0/AN0) | ADCON0=0x05 (AN1), PCFG=1101 (AN0+AN1 analog), TRISAbits.TRISA1=1; DIP AN1 (posição 10) |

**Resultado:** BUILD SUCCEEDED · trimpot 2 (abaixo RB0/INT) agora controla temperatura via AN1

---

## 2026-06-22 — Trabalho Elevador: 7 fixes (sessão 3 — PICSimLab + LCD animado)

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-005 | `BITMAP_CIMA`/`BITMAP_BAIXO` undefined | Arrays foram renomeados para `BITMAP_UP_A`/`DOWN_A` mas `carregar_chars()` usava nomes antigos | Corrigido nomes + carregados 4 chars CGRAM (frames A/B para ↑ e ↓) |
| 2 | FIX-006 | Motor em PORTA (RA1/RA2/RA3) conflitava com script de apresentação | Sessão anterior moveu motor para PORTA para tentar usar teclado matricial; script diz motor em PORTD | Revertido: MOTOR_IN1=LATD0, MOTOR_IN2=LATD1, PORTA_LED=LATD2 |
| 3 | FIX-007 | Botões dos andares nunca detectados | Teclado matricial usa linhas em PORTD (RD0-RD3) que conflitam com motor; código lia RB4/RB5 (inexistentes) | Removido scanner matricial; leitura direta RB0-RB5 = andares 1-6 (como diz slide 4) |
| 4 | FIX-008 | init_hardware() configurava PORTA para motor mas defines apontavam PORTD | Inconsistência gerada pelas mudanças do FIX-006 | init_hardware() restaurado: RD0/RD1/RD2 outputs, RA0 input (ADC) |
| 5 | FIX-009 | Temperatura sempre 0.0C no PICSimLab | DIP switch AN0 (posição 9) desabilitado → trimpot desconectado de RA0 | Instrução: habilitar DIP AN0 no PICGenios no PICSimLab; código ADC estava correto |
| 6 | FIX-010 | TEMPO_PORTA 1000ms muito curto para demo (3 fases de animação ilegíveis) | 400ms/300ms/300ms não dava tempo de ler as mensagens | Aumentado TEMPO_PORTA=3000ms; fases: 1000ms abrindo / 1000ms aberta / 1000ms fechando |
| 7 | FIX-011 | LCD sem mensagens de estado — só exibia "Andar: X" e temperatura | atualizar_lcd() original não tinha contexto de estado | Reescrito com: "Chamado: Ax" (1,5s), "Subindo...." animado, "Abrindo porta" / "Porta aberta!" / "Fechando porta", "Temp elevada!" |

**Resultado:** BUILD SUCCEEDED (3 warnings inofensivos) · `noboro.hex` atualizado

---

## 2026-06-22 — Trabalho Elevador: 2 fixes de compilação

| # | ID | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | FIX-003 | Error [1111]: ISR declarada após uso | C18 exige forward declaration de funções usadas antes da definição | Adicionado `void Isr_Alta(void);` no topo do arquivo |
| 2 | FIX-004 | ler_adc() trava em PICSimLab (loop infinito) | PICGenios simula PIC18F452 — ADCON2 inexistente, bit GO/DONE nunca seta | Adicionado timeout counter: `while(ADCON0bits.GO && --timeout)` |

**Resultado:** BUILD SUCCEEDED (warnings inofensivos apenas) · `noboro.hex` gerado em `OneDrive/Documentos/Micro-controladores/`
**Nota:** FIX-004 contorna PICSimLab apenas — hardware real (PIC18F4550) não tem esse problema.

---

## 2026-06-07 — Atividade 05: 10 erros de design + 3 erros de compilação

### Erros de design encontrados na revisão do código do colega

| # | Severidade | Bug | Causa raiz | Solução |
|---|---|---|---|---|
| 1 | Crítico | RS mapeado em RE0 em vez de RE2 | Confundiu RS com R/W; padrão McLab2 é RE0=R/W, RE1=EN, RE2=RS | `#define RS PORTEbits.RE2` |
| 2 | Crítico | TRISE2 nunca configurado como saída | Consequência do erro #1 | `TRISEbits.TRISE2 = 0` |
| 3 | Importante | Fuse CPUDIV ausente | Sem CPUDIV o divisor de clock fica indefinido, afetando baud rate e delays | `#pragma config CPUDIV = OSC1_PLL2` |
| 4 | Importante | LCD_Write_Char sem delay após EN=0 | HD44780 precisa ~40µs para processar cada char; escrita rápida corrompe texto | `delay_us(40)` após último pulso EN |
| 5 | Importante | Sequência de init 4-bit incompleta | `LCD_Cmd(0x02)` é "Return Home", não inicialização 4-bit; falha no hardware real | Sequência nibble 0x3 × 3 + nibble 0x2 conforme Fig. 8.3 |
| 6 | Importante | Delay power-on do LCD: 15ms | Mínimo do datasheet é 20ms | `delay_ms(20)` |
| 7 | Importante | Entry Mode Set 0x06 ausente | Sem 0x06 o cursor não avança após cada escrita | `LCD_Cmd(0x06)` após inicialização |
| 8 | Moderado | IPEN=0 fora do padrão do curso | Professor usa IPEN=1 + RCIP + GIE em todos os exemplos EUSART | `IPR1bits.RCIP=1; RCONbits.IPEN=1; INTCONbits.GIE=1` |
| 9 | Moderado | OERR não tratado na ISR | Se FIFO transbordar, OERR=1 trava a recepção permanentemente | Toggle CREN no início da ISR |
| 10 | Menor | Fuses BOR/BORV ausentes | Fora do padrão dos exemplos do curso | `#pragma config BOR=ON` + `BORV=1` |

### Erros de compilação encontrados ao rodar no MPLAB C18 v3.47

| # | Tipo | Linha | Bug | Causa raiz | Solução |
|---|---|---|---|---|---|
| B1 | Build Error | 65 | `char dado_recebido = RCREG;` dentro de bloco `if` | C18 é C89/C90; declarações só no topo da função | Mover `char dado_recebido;` para o topo de `Isr_Alta` |
| B2 | Warning 2066 | 76, 81 | `strcpypgm2ram(buf, (const rom char *)"literal")` | Cast não resolve o espaço de memória do literal no C18 | Declarar `const rom char MSG[] = "..."` e passar sem cast |
| B3 | Linker Error | — | `LCD_Write_String_RAM` not found | Arquivo `.c` estava truncado na linha 193; funções do final não existiam | Adicionar funções `LCD_Write_String_RAM` e `LCD_Write_String_ROM` ao final |
