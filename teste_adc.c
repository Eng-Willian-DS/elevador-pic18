/*
 * TESTE ADC -- Trimpot ANAL0 -> Temperatura no LCD
 * Baseado no Exercicio 20 do professor (11-adc.md)
 * ELE1012 -- Microcontroladores -- UTFPR Medianeira
 * PIC18F4550 | Fosc = 20 MHz | MPLAB v8.92 + C18 v3.47
 *
 * DIP SWITCHES:
 *   Fileira SUPERIOR pos.9  (ANAL0) = ON
 *   Fileira INFERIOR pos.1  (LCD)   = ON
 *
 * LCD linha 1: "Raw:  XXXX      "
 * LCD linha 2: "Temp: XX.X  C   "
 */

#include <p18f4550.h>
#include <delays.h>
#include <string.h>
#include <stdio.h>
#include "C:\PIC18\biblioteca_lcd_2x16.h"

/****************************************************************************
Centro de Tecnologia Microgenios
Programa: Diplay_7_seg_01
Placa: KIT PICGENIOS
Objetivo: este programa tem por função ler o canal AD0 e AD1 e escrever no lcd
o valor de conversão
Cristal = 4MHz
*******************************************************************************
*/
char texto[16];
int temp_res = 0;
int temp_res2 = 0;
void main() {
 trisb = 0; //define portb como saida
 trisd = 0; //define portd como saida
 ADCON1 = 0x06; //torna todos os pinos AD como i/o de uso geral
 Lcd8_Config(&PORTE,&PORTD,2,1,4,7,6,5,4,3,2,1,0); //inicializa lcd
 Lcd8_Cmd(Lcd_Clear); //apaga lcd
 Lcd8_Cmd(LCD_CURSOR_OFF); //desliga cursor do lcd
 Lcd8_Out(1, 1, "Canal AN0: "); //escreve mansagem na linha 1, coluna 1 do lcd
 delay_ms (10); //delay de 10ms
 Lcd8_Out(2, 1, "Canal AN1: "); //escreve mensagem na linha 2, coluna 1 do lcd
 delay_ms (10); //delay 10 milisegundos
 ADCON1 = 0b00001110; //habilita canal A/D 0 e A/D1 do PIC
 trisa=0b00001111; //define pinos como entrada
 do
 {
 temp_res = Adc_Read(0); //le canal ad0 do PIC e salva valor na variável temp_res
 temp_res2 = adc_read(1); //lê canal ad1 do PIC e salva valor na variável temp_res2
 Delay_10us; //delay de 10 microsegundos
 wordToStr(temp_res, texto); //converte valor da conversão do ad0 para string
 lcd8_out(1,11,texto); //escreve no lcd o valor da conversão do ad0
 delay_us(10); //delay de 10 us
 WordToStr(temp_res2, texto); //converte valor da conversão do ad1 para string
 lcd8_out(2,11,texto); //escreve no lcd o valor da conversão do ad1
}
 while (1);
}