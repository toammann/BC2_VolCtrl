/*
 * cmd_action.c
 *
 * Created: 13.04.2019 15:10:54
 *  Author: holzi
 */ 

#include "../volctrl.h"
#include "cmd.h"
#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
#include "../UART/uart.h"

#include <avr/interrupt.h>

command cmd_set[NUM_CMDS] ={{0, &volup, "volup"},
							{0, &voldown, "voldown"},
							{2, &setvolume, "setvol"}};

volatile uint16_t adc_val;

void inc_timer_start (void){
	if (inc_cnt_stat == FALSE){
		//Reset Timer count register
		TCNT3 = 0;
		
		//Set the prescaler value to start the counter
		TCCR3B |= TIMER3_PRESCALER_VAL;
		
		//Set timer status flag
		inc_cnt_stat = TRUE;
	}
}

void inc_timer_stop (void){
	//clear the prescaler value to stop the counter
	TCCR3B &= ~TIMER3_PRESCALER_VAL;
	
	inc_cnt_stat = FALSE;
}

void inc_timer_rst (void){
	//Reset Timer count register
	TCNT3 = 0;
}

uint8_t get_motor_stat(void){
	uint8_t pin_motor_cw = (PIND & (1 << PIN_MOTOR_CW));
	uint8_t pin_motor_ccw = (PIND & (1 << PIN_MOTOR_CCW));
	
	uint8_t motor_stat = ((pin_motor_cw >> PIN_MOTOR_CW) << 1) | (pin_motor_ccw >> PIN_MOTOR_CCW);
	
	switch (motor_stat){
		case 0b00:
			return MOTOR_STAT_OFF;
			break;
		case 0b10:
			return MOTOR_STAT_CW;
			break;
		case 0b01:
			return MOTOR_STAT_CCW;
			break;
		default: 
			//ERROR
			error_led(TRUE);
			break;
	}
	return -1;
}

void set_motor_off (void){
	//Set both motor ctrl pins to low
	PORTD &=  ~(1 << PIN_MOTOR_CW);
	PORTD &=  ~(1 << PIN_MOTOR_CCW);
}

uint8_t chk_adc_range(uint16_t val)
{
	if (val < ADC_POT_LO_TH ){
		//Motor potentiometer left limit
		return ADC_POT_STAT_LO;
	} else if (val > ADC_POT_HI_TH){
		//Motor potentiometer right limit
		return ADC_POT_STAT_HI;
	}
	else {
		//Motor not at limit
		return ADC_POT_STAT_OK;
	}
}

void set_motor_ccw (void){
	
	switch (get_motor_stat()){
		case MOTOR_STAT_OFF:
			//Motor was off -> turn on in ccw direction
			PORTD &=  ~(1 << PIN_MOTOR_CW);		//0
			PORTD |=  (1 << PIN_MOTOR_CCW);		//1
			break;
			
		case MOTOR_STAT_CCW:
			//Motor is already turning in CCW direction
			//do noting...
			break;

		case MOTOR_STAT_CW:
			//Motor is turning CW -> change rotation direction
			set_motor_off();
			_delay_ms(MOTOR_OFF_DELAY_MS);
			PORTD &=  ~(1 << PIN_MOTOR_CW);		//0
			PORTD |=  (1 << PIN_MOTOR_CCW);		//1
			break;

		default: 
			error_led(TRUE); //Error
			break;
	}
}

void set_motor_cw (void){
	
	switch (get_motor_stat()){
		case MOTOR_STAT_OFF:
			//Motor was off -> turn on in cw direction
			PORTD |= (1 << PIN_MOTOR_CW);		//1
			PORTD &=  ~(1 << PIN_MOTOR_CCW);	//0
			break;
		
		case MOTOR_STAT_CCW:
			//Motor is turning CCW -> change rotation direction
			set_motor_off();
			_delay_ms(MOTOR_OFF_DELAY_MS);
			PORTD |= (1 << PIN_MOTOR_CW);		//1
			PORTD &=  ~(1 << PIN_MOTOR_CCW);	//0
			break;
			
		case MOTOR_STAT_CW:
			//Motor is already turning in CW direction
			//do noting...
			break;
		default: break;
	}
}

void volup(uint8_t argc, char *argv[]){
	
	uart0_puts("volup detected\r\n");
	
	//Check if the Motor is at the upper (right) limit
	if (chk_adc_range(adc_val) == ADC_POT_STAT_HI){
		//Motor potentiometer is at right limit
		if (DEBUG_MSG) {
			uart0_puts("Motor @ upper lim!\r\n");
		}
		//Do not turn the Motor on
		return;
	}
		
	if (inc_cnt_stat == FALSE){
		//Timer not running
		inc_timer_start();	//Start increment timer
		
		//Start motor if it is not already running
		if ( get_motor_stat() == MOTOR_STAT_OFF ){
			set_motor_cw();		//Start motor in cw direction (volup)
		} else {
			//ERROR: running motor without a timer is not allowed turn on error indicator LED
			error_led(TRUE);
			set_motor_off();
		}
	} 
	else {
		//Timer already running (e.g from a previous volup cmd)
		inc_timer_rst(); // restart timer
		
		//Rotation direction is unknown here -> handled in the set_motor_functions
		set_motor_cw();		//Start motor in cw direction (volup)
	}

	//The motor and timer is stopped by the timer ISR
	
	//Start motor
	
	// prüfe ob motor schon läuft
	// starte timer entsprechend neu
	
	//motor dreht anders rum..
	//-> stoppe motor
	//-> starte motor anders rum
	//-> starte timer


	//timer stoppt den motor!
	//notfall timer implementieren
}

void voldown(uint8_t argc, char *argv[]){
	uart0_puts("voldown detected\r\n");
	
	//Check if the Motor is at the lower (left) limit
	if (chk_adc_range(adc_val) == ADC_POT_STAT_LO){
		//Motor potentiometer is at right limit
		if (DEBUG_MSG) {
			uart0_puts("Motor @ lower lim!\r\n");
		}
		//Do not turn the Motor on
		return;
	}
	
	if (inc_cnt_stat == FALSE){
		//Timer not running
		inc_timer_start();	//Start increment timer
		
		//Start motor if it is not already running
		if ( get_motor_stat() == MOTOR_STAT_OFF ){
			set_motor_ccw();		//Start motor in ccw direction (voldown)
			} else {
			//ERROR: running motor without a timer is not allowed turn on error indicator LED
			error_led(TRUE);
			set_motor_off();
		}
	}
	else {
		//Timer already running (e.g from a previous volup cmd)
		inc_timer_rst(); // restart timer
		
		//Rotation direction is unknown here -> handled in the set_motor_functions
		//A change of the rotation direction is possible
		set_motor_ccw();		//Start motor in cw direction (volup)
	}
}

void setvolume(uint8_t argc, char *argv[]){
	char buffer[5];
		
	uart0_puts("setvolume detected\r\n");
		
	uart0_puts("argc: ");
	uart0_puts(itoa(argc, buffer, 10));
	uart0_puts("\r\n");
		
	for (int i=0; i < argc; i++)
	{
		uart0_puts("argv: ");
		uart0_puts(argv[i]);
		uart0_puts("\r\n");
	}
}

ISR(TIMER3_COMPA_vect)
{
	PORTC ^= ( 1<< PORTC2);
	//every INC_DURATION ms
	set_motor_off();
	inc_timer_stop();
}
