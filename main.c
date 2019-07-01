/*
 * VolCtrl_FW.c
 *
 * Created: 07.03.2019 23:49:44
 * Author : holzi
 */ 

#include "volctrl.h"

#include <avr/io.h>
#include "avr/interrupt.h"
#include "./UART/uart.h"
#include "./CMD/cmdparser.h"
#include <inttypes.h>
#include <string.h>
#include <util/delay.h>
#include "./IMRP/irmp.h"

#include "stdlib.h"

//GLOBAL VARIABLES
uint8_t inc_cnt_stat = 0;
uint8_t motor_stat = MOTOR_STAT_OFF;
uint8_t FSM_STATE = 0;
volatile uint16_t adc_val = 0;


uint8_t ir_cmd_volup = 16;
uint8_t ir_cmd_voldown = 17;


#define STATE_INIT 0
#define STATE_UART_CMD_REC 1
#define STATE_IR_CMD_REC 2
#define CMD_PROC 3

uint8_t CMD_REC_UART = 0;
uint8_t CMD_REC_IR = 0;




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



//timer 0 für fehler überwachung

//Volume increment counter
void timer3_init (void){
	//16Bit Timer
	//TC1 Control Register B
	//Mode 4 - CTC (clear timer on compare)
	TCCR3B =  (1 << WGM32);
	
	// Set prescaler to 256
	//TCCR3B =  (1 << WGM32) | (1 << CS31) | (1 << CS30); //64
	//TCCR3B =  (1 << WGM32) | (1 << CS32);

	//Do not set the prescaler -> timer is not started!

	//Output Compare Register 3 A Low and High byte 
	//Sets the counter value at which the interrupt gets executed
	OCR3A = (uint16_t) TIMER_COMP_VAL(TIMER3_PRESCALER, INC_DURATION); //16Bit value is correct addressed automatically
	//OCR3A = 23437; //for 500ms
	
	//Counter 3 Interrupt Mask Register
	//Set OCIE3A Flag: Timer/Counter 3, Output Compare A Match Interrupt Enable
 	TIMSK3 = (1 << OCIE3A);
}

//IRMP Timer		
static void timer1_init (void)
{     
	//16Bit timer
	OCR1A   =  (F_CPU / F_INTERRUPTS) - 1;	// compare value: 1/15000 of CPU frequency
	TCCR1B  = (1 << WGM12) | (1 << CS10);   // switch CTC Mode on, set prescaler to 1
	TIMSK1  = 1 << OCIE1A;                  // OCIE1A: Interrupt by timer compare
}		
				
// Timer1 output compare A interrupt service routine, called every 1/15000 sec		
ISR(TIMER1_COMPA_vect)
{
	(void) irmp_ISR();	//Call IRMP ISR
}
	
static char * itoh (char * buf, uint8_t digits, uint16_t number)
{
	for (buf[digits] = 0; digits--; number >>= 4)
	{
		buf[digits] = "0123456789ABCDEF"[number & 0x0F];
	}
	return buf;
}

void adc0_init(){
	//AREF, Internal Vref turned OFF
	//ADC MUX = 0 (ADC0)
	ADMUX = 0x00;
	
	//Enable ADC
	//ADC Interrupt Enable
	//Division Factor = 128
	//
	ADCSRA = ( 1 << ADEN) | (1 << ADIE) | 
			 ( 1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2) |
			 ( 1 << ADATE);
	//ADCSRB = 0; //Free Running mode
	
	//Start conversion (first result is invalid)
	ADCSRA |= (1 << ADSC);
	  
	//Disable digital input buffer
	DIDR0 = (1 << ADC0D);
}

ISR(ADC_vect){
	//Read ADC Value
	adc_val = ADC;
}

					
int main(void)
{
	IRMP_DATA   irmp_data;
    irmp_init(); // initialize IRMP
	timer1_init(); //IMRC
	timer3_init(); //Motor Ctrl
	
	
	char buf[10];
	
	uart0_init(UART_BAUD_SELECT(57600, F_CPU));
	//uart1_init(UART_BAUD_SELECT(57600, F_CPU));

	_delay_ms(200);		//wait until the boot message of ESP8266 at 74880 baud has passed
	
	uart0_puts("boot!");
	
	//Pin Configurations
	//Direction Control Register (1=output, 0=input)
	
	DDRB = (1 << ERROR_LED); 
	DDRC = (1 << PORTC2);
	DDRD = (1 << PIN_MOTOR_CW) | (1 << PIN_MOTOR_CCW);

	//Pullup Config
	PORTD = 0; //Deactivate pullups

	error_led(FALSE); //Turn off Error LED
	
	adc0_init();

	sei();

	while (1)
	{

		//uart0_puts(itoa(adc_val, buf, 10));
		//uart0_puts("\r\n");
		
		
		if (uart0_getln(uart0_line_buf) == GET_LN_RECEIVED){
			//got a command via UART (WiFi)
			//cmd_parser(uart0_line_buf);
			CMD_REC_UART = 1;
		}
		
		if (irmp_get_data (&irmp_data)){
			// got an IR message
			CMD_REC_IR = 1;
		}
		
		switch ( FSM_STATE ) {
			case STATE_INIT:
				//Init State, do nothing (waiting for commands)
				
				if (CMD_REC_UART) {
					FSM_STATE = STATE_UART_CMD_REC;
					CMD_REC_UART = 0;
				}
				
				if (CMD_REC_IR){
					FSM_STATE = STATE_IR_CMD_REC;
					
				#if DEBUG_MSG
					uart0_puts ("protocol: 0x");
					itoh (buf, 2, irmp_data.protocol);
					uart0_puts (buf);
				
					uart0_puts ("   address: 0x");
					itoh (buf, 4, irmp_data.address);
					uart0_puts (buf);
				
					uart0_puts ("   command: 0x");
					itoh (buf, 4, irmp_data.command);
					uart0_puts (buf);
				
					uart0_puts ("   flags: 0x");
					itoh (buf, 2, irmp_data.flags);
					uart0_puts (buf);
					uart0_puts("\r\n");
					CMD_REC_IR = 0;
				}
				#endif
				break;
			
			case STATE_UART_CMD_REC:
				//Call CMD parser
				cmd_parser(uart0_line_buf);

				FSM_STATE = CMD_PROC;
				break;
				
			case STATE_IR_CMD_REC:
				//Check received ir command
				if (irmp_data.command == ir_cmd_volup){
					volup(0, NULL);
				} 
				else if (irmp_data.command == ir_cmd_voldown){
					voldown(0,NULL);
				}
				FSM_STATE = CMD_PROC;
				break;
				
			case CMD_PROC:
				//wait for cmd to end
				//react to new incomming commands
				
				// prüfe den anschlags adc
					//anschlag erreicht
					//stoppe timer
					//gehe zu init
					
					
				//Timer flag muss anderswo gecleared werden
					
				
				//timer abgelaufen oder nie gestartet
				FSM_STATE = STATE_INIT;
				break;
				
			default: break;
		}
	}
}