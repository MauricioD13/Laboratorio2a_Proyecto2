/*
 * File:   main.c
 * Author: guasonito
 *
 * Created on 13 de marzo de 2021, 09:03 AM
 */

#include "timers/timers.h"
#include "adc/adc.h"
#include "uart/uart.h"
#include "spi/spi.h"
#include "filter/filter.h"
#include <xc.h>
#include <PIC18F47K42.h>

// CONFIG1L
#pragma config FEXTOSC = OFF    // External Oscillator Selection (Oscillator not enabled)
#pragma config RSTOSC = HFINTOSC_64MHZ// Reset Oscillator Selection (HFINTOSC with HFFRQ = 64 MHz and CDIV = 1:1)

// CONFIG1H
#pragma config CLKOUTEN = OFF   // Clock out Enable bit (CLKOUT function is disabled)
#pragma config PR1WAY = ON     // PRLOCKED One-Way Set Enable bit (PRLOCK bit can be cleared and set only once)
#pragma config CSWEN = OFF      // Clock Switch Enable bit (Writing to NOSC and NDIV is allowed)
#pragma config FCMEN = ON       // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor enabled)

// CONFIG2L
#pragma config MCLRE = EXTMCLR  // MCLR Enable bit (If LVP = 0, MCLR pin is MCLR; If LVP = 1, RE3 pin function is MCLR )
#pragma config PWRTS = PWRT_OFF // Power-up timer selection bits (PWRT is disabled)
#pragma config MVECEN = ON      // Multi-vector enable bit (Multi-vector enabled, Vector table used for interrupts)
#pragma config IVT1WAY = ON     // IVTLOCK bit One-way set enable bit (IVTLOCK bit can be cleared and set only once)
#pragma config LPBOREN = OFF    // Low Power BOR Enable bit (ULPBOR disabled)
#pragma config BOREN = SBORDIS  // Brown-out Reset Enable bits (Brown-out Reset enabled , SBOREN bit is ignored)

// CONFIG2H
#pragma config BORV = VBOR_2P45 // Brown-out Reset Voltage Selection bits (Brown-out Reset Voltage (VBOR) set to 2.45V)
#pragma config ZCD = OFF        // ZCD Disable bit (ZCD disabled. ZCD can be enabled by setting the ZCDSEN bit of ZCDCON)
#pragma config PPS1WAY = ON     // PPSLOCK bit One-Way Set Enable bit (PPSLOCK bit can be cleared and set only once; PPS registers remain locked after one clear/set cycle)
#pragma config STVREN = ON     // Stack Full/Underflow Reset Enable bit (Stack full/underflow will cause Reset)
#pragma config DEBUG = OFF      // Debugger Enable bit (Background debugger disabled)
#pragma config XINST = OFF      // Extended Instruction Set Enable bit (Extended Instruction Set and Indexed Addressing Mode disabled)

// CONFIG3L
#pragma config WDTCPS = WDTCPS_31// WDT Period selection bits (Divider ratio 1:65536; software control of WDTPS)
#pragma config WDTE = OFF       // WDT operating mode (WDT Disabled; SWDTEN is ignored)

// CONFIG3H
#pragma config WDTCWS = WDTCWS_7// WDT Window Select bits (window always open (100%); software control; keyed access not required)
#pragma config WDTCCS = SC      // WDT input clock selector (Software Control)

// CONFIG4L
#pragma config BBSIZE = BBSIZE_512// Boot Block Size selection bits (Boot Block size is 512 words)
#pragma config BBEN = OFF       // Boot Block enable bit (Boot block disabled)
#pragma config SAFEN = OFF      // Storage Area Flash enable bit (SAF disabled)
#pragma config WRTAPP = OFF     // Application Block write protection bit (Application Block not write protected)

// CONFIG4H
#pragma config WRTB = OFF       // Configuration Register Write Protection bit (Configuration registers (300000-30000Bh) not write-protected)
#pragma config WRTC = OFF       // Boot Block Write Protection bit (Boot Block (000000-0007FFh) not write-protected)
#pragma config WRTD = OFF       // Data EEPROM Write Protection bit (Data EEPROM not write-protected)
#pragma config WRTSAF = OFF     // SAF Write protection bit (SAF not Write Protected)
#pragma config LVP = ON         // Low Voltage Programming Enable bit (Low voltage programming enabled. MCLR/VPP pin function is MCLR. MCLRE configuration bit is ignored)

// CONFIG5L
#pragma config CP = OFF         // PFM and Data EEPROM Code Protection bit (PFM and Data EEPROM code protection disabled)

// CONFIG5H

// #pragma config statements should precede project file includes.
// Use project enums instead of #define for ON and OFF.

//GLOBAL VARIABLES 
STATES states;
COUNTERS counters;
int received = 10;
int rx;
int aux_states;
int cont = 0;
char MSB_spi;
char LSB_spi;
int aux;
short int aux1;
short int aux2;
int spi_result;
int x = 0;
coef_iir_2_ord ir;

//Timer interrupt
void __interrupt(irq(IRQ_TMR0),base(0x0008)) T0_isr(){
    //1us
    
    counters.counter == counters.count_to ? START_CONVERSION = 1,PORTDbits.RD1 = 1,counters.counter = 0:counters.counter++;
    PORTDbits.RD1 = 0;
    PIR3bits.TMR0IF = 0;
    
    
}

//ADC Interrupt
void __interrupt(irq(IRQ_AD),base(0x0008)) ADC_isr(){
    states.ADC_number =  read_ADC();
    states.read_ADC_flag = 1;
    PIR1bits.ADIF = 0;
}
void __interrupt(irq(IRQ_U1RX),base(0x0008)) U1RX_isr(){
    
    rx = U1RXB;
    counters.cont_rx++;
            
    //change_parameters(&counters,&received);
    PIR3bits.U1RXIF = 0;
}
void __interrupt(irq(IRQ_SPI1TX),base(0x0008)) SPI_isr(){
    
    
    PORTBbits.RB4 = 1;
    if(cont == 0){
        PORTBbits.RB4 = 0;
        PORTBbits.RB4 = 1;
        PORTEbits.RE0 = 1;
        PORTEbits.RE0 = 0;
        
        SPI1TXB = LSB_spi;
        cont++;
        
    }
    else if(cont == 1){
        
        SPI1TXB = MSB_spi;
        cont = 0;
        
    }
    
    
    PIR2bits.SPI1TXIF = 0;
    
}

void convert_number(STATES *states){
    double voltage  = (double)(0.001220703125)*states->ADC_number; 
    (states->integer) = (short int) (voltage);
    (states->decimal_one)= (short int)((voltage*10)-((states->integer)*10));
    (states->decimal_two) = (short int)((voltage*100)-((states->integer *100)+(states->decimal_one *10)));
    }



void init_PIC(void){
    config_T0();
    config_ADC();
    config_UART();
    config_SPI();
    
   
}

int main(void) {
    float num[5] = { 0.9536743164063,    1.907348632813,    2.861022949219,    1.907348632813,
     0.9536743164063};
    float den[5] = {1,          1.953125,      2.8583984375,    1.861572265625,
      0.908447265625};
    float w[5] = {0,0,0,0,0};
    INTCON0 = 0x80; 
    int status_tx = 0;
    oscillator_module();
    counters.count_to = (25 *received) - ((received*2)-1);
    //44 IIR
    //+-23 FIR
    states.spi_transmit = 0;
   // SPI1TXB = 57;
    init_PIC();
    inicializar_iir(num,den,w,&ir);
   
    
    //Ports to measure time
    TRISDbits.TRISD0 = 0;
    ANSELDbits.ANSELD0 = 0;
    TRISDbits.TRISD1 = 0;
    ANSELDbits.ANSELD1 = 0;
    
    
    //SPI1TXB = 0x1F;
    //PORTEbits.RE0 = 1;    
    
    //MAIN LOOP
    while(1){
        aux = states.filtered_number_IIR + 4096;
        
        LSB_spi = (char)aux; 
        aux = aux>>8;
        MSB_spi = (char)aux;
            
        if(states.read_ADC_flag == 1){
            
            convert_number(&states);
            
            if(TX_FLAG == 1){
                
                counters.cont_tx++;
                //Serial communication works sending each separate by '-' and when the number ends it send '#'
                if(counters.cont_tx == 1){
                    x = (char)states.filtered_number_IIR>>8;
                    TO_TRANSMIT = (char)x;
                }
                else if (counters.cont_tx == 2){
                    
                    TO_TRANSMIT = (char)states.filtered_number_IIR;
                    states.value_transmitted = 1;
                    counters.cont_tx = 0;
                }
                TX_FLAG = 0;
                
            }
            //UART SEND
            /*if(TX_FLAG == 1){
                
                counters.cont_tx++;
                //Serial communication works sending each separate by '-' and when the number ends it send '#'
                if(counters.cont_tx == 1){
                    status_tx = transmit_UART(states.integer);
                }
                else if (counters.cont_tx == 2){
                    status_tx = transmit_UART(45);
                }
                else if (counters.cont_tx == 3){
                    status_tx = transmit_UART(states.decimal_one);
                }
                else if (counters.cont_tx == 4){
                    status_tx = transmit_UART(45);
                }
                else if (counters.cont_tx == 5){
                    status_tx = transmit_UART(states.decimal_two);
                }
                else if (counters.cont_tx == 6){
                    status_tx = transmit_UART(35);
                    states.value_transmitted = 1;
                    counters.cont_tx = 0;
                }
                
                TX_FLAG = 0;
                
            }*/
            
            //states.filtered_number_FIR = filter_FIR(states.ADC_number);
            states.filtered_number_IIR = (int)filtrarIIR((float)states.ADC_number,&ir);
        }
        
        if(counters.cont_rx == 1){
            received = rx;
            }
            else if(counters.cont_rx == 2){
                rx = (rx<<8);
                received = (received) |(rx);
                counters.count_to = (10*received) - (received-1);
                counters.counter = 0;
                counters.cont_rx = 0;
                rx = 0;
        }
        
        
    
    }
    
}
