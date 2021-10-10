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

#define FAN_1 PORTDbits.RD4 = PORTAbits.RA0
#define FAN_2 PORTDbits.RD5 = PORTBbits.RB5
#define HEATER_1 PORTDbits.RD6
#define HEATER_2 PORTDbits.RD7
#define ALARM PORTEbits.RE1
#define WATER PORTBbits.RB3

#define BUZZER_HOT_START 250
#define BUZZER_COLD_START 5

#define TMR2_OFF T2CON &= 0xFB
#define TMR2_ON T2CON |= 0x04; buzzer_on = 1

const uint8_t led_numbers[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 
    0x7F, 0x67, 0x77};
uint8_t display_position = 0;

const double resolution = 48.87585533;
uint16_t adc_value = 0;
uint32_t adc_read = 0;
uint32_t first_temperature = 0;
uint32_t calc_temperature = 0;
uint32_t past_temperature = 0;
uint32_t temperature = 0;

uint16_t water_count = 0;

/*
 * Variables in charge of controlling buzzer behavior. 
 * - buzzer_on: Whether buzzer should be on or off. 
 * - buzzer_type: Used to set PR2 and therefore change frequency based on
 * whether the alarm is being used for heat or cold.
 * - buzzer_state: State of the pin used to drive the buzzer
 */
uint8_t buzzer_on = 0;
uint8_t buzzer_type = 0;
uint8_t buzz_state = 0;

void read_adc() 
{
    adc_value = 0;
    ADCON0bits.GO_nDONE = 1;
    while (ADCON0bits.GO_nDONE == 1) {}
    NOP();
    adc_value |= ADRESH;
    adc_value = adc_value << 2;
    adc_value |= (ADRESL >> 6);
    
    adc_read = (uint16_t) (adc_value * resolution);
}

void read_temperature() {
    calc_temperature = 0;
    read_adc();
    first_temperature = adc_read;
    calc_temperature += first_temperature;
    
    for (uint16_t sample = 0; sample != 300; sample++) {
        read_adc();
        calc_temperature += adc_read;
    }
    
    read_adc();
    calc_temperature += (adc_read - first_temperature);
    calc_temperature /= 301;
    
    past_temperature = temperature;
    temperature = calc_temperature;
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
        PIR1bits.TMR2IF = 0;
        if (buzzer_on == 0) {
            buzz_state = 0;
            ALARM = buzz_state;
        }
        else {
           buzz_state = !buzz_state;
            ALARM = buzz_state;
            if (buzzer_type == 0) {
                PR2 = BUZZER_HOT_START;
            }
            else {
                PR2 = BUZZER_COLD_START;
            }
            TMR2_ON; 
        }
    }
    
    if ((INTCONbits.TMR0IE == 1) && (INTCONbits.T0IF == 1)) {
        water_count++;
        if (water_count == 1875) {
            INTCON &= 0xDF;
            water_count = 0;
            WATER = 0;
        }
        INTCONbits.T0IF = 0;
    }
}

void main(void) 
{
    OSCCON = 0x79;
    
    ANSEL = 0x80;
    ANSELH = 0x00;
    ADCON0 = 0x9D;
    ADCON1 = 0x00;
    ADCON0 |= 0x01;
    
    PIE1bits.TMR1IE = 1;
    PIE1bits.TMR2IE = 1;
    INTCONbits.PEIE = 1;
    INTCONbits.GIE = 1;

    PORTA = 0x00;
    TRISA = 0x00;
    PORTB = 0x00;
    TRISB = 0x00;
    PORTC = 0x00;
    TRISC = 0x00;
    PORTD = 0x00;
    TRISD = 0x00;
    PORTE = 0x00;
    TRISE = 0x0C;
    OPTION_REG = 0xD7;
    T2CON = 0x79;

    __delay_us(100);
    read_temperature();
    T1CON = 0xB1;
    while (1)
    {
        read_temperature(); 
        if (temperature >= 4500) {
            FAN_1 = 1;
            if (temperature >= 6000) {
                FAN_2 = 1;
                buzzer_type = 0;
                TMR2_ON;
            }
            if (temperature < 6000) {
                FAN_2 = 0;
                buzzer_on = 0;
            }
        }
        else {
            FAN_1 = 0;
        }
        if (water_count == 0) {
            if ((past_temperature < 3000) && (temperature >= 3000)) {
                WATER = 1;    
                INTCON |= 0x20; 
            }
            else if ((past_temperature > 3000) && (temperature <= 3000)) {
                WATER = 1;    
                INTCON |= 0x20; 
            }
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
            buzzer_on = 0;
        }   
    }
    
}