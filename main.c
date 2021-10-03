#include <xc.h>
#include <stdint.h>

#pragma config DEBUG = 1
#pragma config LVP = 0
#pragma config FCMEN = 0
#pragma config IESO = 0
#pragma config BOREN = 00
#pragma config CPD = 0
#pragma config CP = 0
#pragma config MCLRE = 0
#pragma config PWRTE = 1
#pragma config WDTE = 0
#pragma config FOSC = 101

#define _XTAL_FREQ 8000000
#define TMR1_START_H 0xF7
#define TMR1_START_L 0xDA

#define DISPLAY PORTC
#define DISPLAY_SELECT PORTD


const uint8_t led_numbers[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 
    0x7F, 0x67, 0x77};
uint8_t display_position = 0;

const double resolution = 48.8758533;
uint16_t adc_value = 0;
uint16_t temperature = 2025;

void read_adc() 
{
    adc_value = 0;
    ADCON0bits.GO_nDONE = 1;
    while (ADCON0bits.GO_nDONE == 1) {}
    NOP();
    adc_value |= ADRESH;
    adc_value = adc_value << 2;
    adc_value |= (ADRESL >> 6);
    
    temperature = (uint16_t) (adc_value * resolution);
}

void display_number() 
{
    DISPLAY_SELECT |= 0x0F;
    if (display_position == 0) {
        DISPLAY = led_numbers[(uint8_t) (temperature % 10)];
        DISPLAY_SELECT &= 0xFE;
    }
    else if (display_position == 1) {
        DISPLAY = led_numbers[(uint8_t) ((temperature / 10) % 10)];
        DISPLAY_SELECT &= 0xFD;
    }
    else if (display_position == 2) {
        DISPLAY = led_numbers[(uint8_t) ((temperature / 100) % 10)];
        DISPLAY |= 0x80;
        DISPLAY_SELECT &= 0xFB;
    }
    else {
        DISPLAY = led_numbers[(uint8_t) (temperature / 1000)];
        DISPLAY_SELECT &= 0xF7;
    }
}

void __interrupt() handle_int(void)
{
    if ((PIE1bits.TMR1IE == 1) && (PIR1bits.TMR1IF == 1)) {
        T1CON &= 0xFE;
        
        display_number();
        display_position++;
        if (display_position > 3) {
            display_position = 0;
        }
       
        TMR1H = TMR1_START_H;
        TMR1L = TMR1_START_L;
        PIR1bits.TMR1IF = 0;
        T1CON |= 0x01;
    }
}

void main(void) 
{
    OSCCON = 0x79;
    
    ADCON0 = 0x9D;
    ADCON1 = 0x00;
    ADCON0 |= 0x01;
    
    PIE1bits.TMR1IE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;
    
    
    PORTC = 0x00;
    TRISC = 0x00;
    PORTD = 0x00;
    TRISD = 0x00;
    TRISE = 0x0F;
    
    T1CON = 0xB1;
    while (1)
    {
        read_adc();
        switch (temperature) {
            case 6000:
                NOP();
            case 4500:
                NOP();
                break;
            case 1000:
                NOP();
            case 1500:
                NOP();
                break;
        }

    }
    
}