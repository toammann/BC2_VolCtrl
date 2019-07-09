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
#include <avr/pgmspace.h>

volatile uint16_t adc_val;




const uint16_t poti_log_curve[] PROGMEM = 
	{	10,  11,  11,  12,  13,  15,  17,
		21,  24,  25,  28,  30,  37,  48,
		59,  69,  79,  90, 102, 114, 125,
		136, 147, 156, 170, 186, 191, 196,
		207, 220, 232, 242, 256, 279, 308,
		341, 383, 438, 500, 561, 622, 683,
		743, 803, 861, 919, 974, 1009, 1020,
		1023, 1023};



void getadcval(uint8_t argc, char *argv[]){
	char buf[10];
	uart0_puts("ADC Value: ");
	uart0_puts(itoa(adc_val, buf, 10));
	uart0_puts("\r\n");
}

void inc_timer_start (void){
	if (inc_timer_stat == FALSE){
		//Reset Timer count register
		TCNT3 = 0;
		
		//Set the prescaler value to start the counter
		TCCR3B |= TIMER3_PRESCALER_VAL;
		
		//Set timer status flag
		inc_timer_stat = TRUE;
	}
}

void inc_timer_stop (void){
	//clear the prescaler value to stop the counter
	TCCR3B &= ~TIMER3_PRESCALER_VAL;
	inc_timer_stat = FALSE;
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
	_delay_ms(MOTOR_OFF_DELAY_MS);
}

uint8_t chk_adc_range(uint16_t val)
{
	if (val <= ADC_POT_LO_TH ){
		//Motor potentiometer left limit
		return ADC_POT_STAT_LO;
	} else if (val >= ADC_POT_HI_TH){
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
			//Motor is turning CW -> Turn off Motor
			set_motor_off();
			inc_timer_stop();
			_delay_ms(MOTOR_OFF_DELAY_MS);
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
			//Motor is turning CCW -> Turn off Motor
			set_motor_off();
			inc_timer_stop();
			_delay_ms(MOTOR_OFF_DELAY_MS);
			break;
			
		case MOTOR_STAT_CW:
			//Motor is already turning in CW direction
			//do noting...
			break;
		default: break;
	}
}

void volup(uint8_t argc, char *argv[]){
	
	//Broadcast a notification via UART
	uart0_puts("volup detected\r\n");
	
	//Change FSM_STATE
	FSM_STATE = STATE_VOLUP;
	
}

void voldown(uint8_t argc, char *argv[]){
	//Broadcast a notification via UART
	uart0_puts("voldown detected\r\n");
	
	//Change FSM_STATE
	FSM_STATE = STATE_VOLDOWN;
}

void setvolume(uint8_t argc, char *argv[]){
	
	uart0_puts("setvolume detected\r\n");

	int idx;
	char buffer[5];

	#if DEBUG_MSG
		uart0_puts("argc: ");
		uart0_puts(itoa(argc, buffer, 10));
		uart0_puts("\r\n");
		
		for (int i=0; i < argc; i++)
		{
			uart0_puts("argv: ");
			uart0_puts(argv[i]);
			uart0_puts("\r\n");
		}
	#endif
	
	//Get integer from argument vector (string)
	idx = atoi( argv[0] );
	

	
	if ( (idx > 100) || (idx < 0) ){
		uart0_puts("Argument out of range!\r\n");
		//error_led(TRUE);
		return;
	}
	
	//Result gets cropped if it not a integer
	//->ne next smaller value is taken. z.B. 99/2=49.5 -> 49
	idx = idx /2;

	//To accsess data from program memory
	//Adress the data as normal -> take the address &()
	//use pgm_read marko
	setvol_targ = pgm_read_word( &(poti_log_curve[idx]) );
	
	//Switch to STATE_SETVOL FSM State
	FSM_STATE = STATE_SETVOL;
	
	#if DEBUG_MSG
		uart0_puts("Target ADC value: ");
		uart0_puts(itoa(setvol_targ, buffer, 10));
		uart0_puts("\r\n");
	#endif
}


void error_led (uint8_t status){
	if ( status ){
		//Turn ERROR LED on
		PORTB |= (1 << ERROR_LED);
	}
	else {
		//Turn ERROR LED off
		PORTB &= ~(1 << ERROR_LED);
	}
}
