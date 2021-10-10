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
#define TMR1_START_H 0xFD
#define TMR1_START_L 0xE8

#define DISPLAY PORTC
#define DISPLAY_SELECT PORTD

#define FAN_1 PORTDbits.RD4
#define FAN_2 PORTDbits.RD5
#define HEATER_1 PORTDbits.RD6
#define HEATER_2 PORTDbits.RD7
#define ALARM PORTEbits.RE1

#define BUZZER_HOT_START 230
#define BUZZER_COLD_START 15

#define TMR2_OFF T2CON &= 0xFB
#define TMR2_ON T2CON |= 0x04

const uint8_t led_numbers[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 
    0x7F, 0x67, 0x77};
uint8_t display_position = 0;

const double resolution = 32.258064;
uint16_t adc_value = 0;
uint16_t temperature = 0;

// 0 is for buzzing when HOT, 1 is for buzzing when cold.
uint8_t buzzer_type = 0;
uint8_t buzz_state = 0;
uint16_t buzz_count = 0;

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
    if (PIR1bits.TMR1IF == 1) {
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
    
    if (PIR1bits.TMR2IF == 1) {
        TMR2_OFF;
        buzz_state = !buzz_state;
        ALARM = buzz_state;
        if (buzzer_type == 0) {
            PR2 = BUZZER_HOT_START;
        }
        else {
            PR2 = BUZZER_COLD_START;
        }
        
        PIR1bits.TMR2IF = 0;
        TMR2_ON;
    }
}

void main(void) 
{
    OSCCON = 0x79;
    
    ADCON0 = 0x9D;
    ADCON1 = 0x00;
    ADCON0 |= 0x01;
    
    PIE1bits.TMR1IE = 1;
    PIE1bits.TMR2IE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

    PORTC = 0x00;
    TRISC = 0x00;
    PORTD = 0x00;
    TRISD = 0x00;
    PORTE = 0x00;
    TRISE = 0x0C;
    T2CON = 0x79;
    T1CON = 0xB1;
            
    while (1)
    {
        read_adc();
        __delay_ms(350);
 
        if (temperature >= 4500) {
            FAN_1 = 1;
            if (temperature >= 6000) {
                FAN_2 = 1;
                buzzer_type = 0;
                TMR2_ON;
            }
            if (temperature < 6000) {
                FAN_2 = 0;
                TMR2_OFF;
            }
        }
        else {
            FAN_1 = 0;
        }
        if (temperature <= 1500) {
            HEATER_1 = 1;
        }
        if (temperature > 1500) {
            HEATER_1 = 0;
        }
        if (temperature <= 1000) {
            HEATER_2 = 1;
            buzzer_type = 1;
            TMR2_ON;
        }
        if (temperature > 1000) {
            HEATER_2 = 0;
        }
        if (temperature > 1000 && temperature < 1600) {
            TMR2_OFF;
        }   
    }
    
}