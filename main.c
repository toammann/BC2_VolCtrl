/*
 * VolCtrl_FW.c
 *
 * Created: 07.03.2019 23:49:44
 * Author : holzi
 */ 
#define F_CPU 12000000UL  // Systemtakt in Hz - Definition als unsigned long beachten

#include <avr/io.h>
#include "avr/interrupt.h"
#include "./UART/uart.h"
#include "./CMD/cmdparser.h"
#include <inttypes.h>
#include <string.h>
#include <util/delay.h>
#include <avr/io.h> //Pins..


//#define F_CPU 12000000UL  // Systemtakt in Hz - Definition als unsigned long beachten
#include "./IMRP/irmp.h"

#define TIMER3_PRESCALER_VAL ((1 << CS30) | (1 << CS31)) //TIMER1 Prescaler=64

//timer 0 für fehler überwachung

//Volume increment counter
void timer3_init (void){
	//16Bit Timer
	//TC1 Control Register B
	//Mode 4 - CTC (clear timer on compare)
	// Set Prescaler to 64
	TCCR3B =  (1 << WGM32) | TIMER3_PRESCALER_VAL;
	
	//Output Compare Register 3 A Low and High byte
	OCR3A = 18750; //16Bit value is correct addressed  automatically
	
	//Counter 3 Interrupt Mask Register
	//Set OCIE3A Flag: Timer/Counter 3, Output Compare A Match Interrupt Enable
	TIMSK3 = (1 << OCIE3A);
}

void inc_counter_start (void){
	//Reset Timer count register
	TCNT3 = 0;
	
	//Set the prescaler value to start the counter
	TCCR3B |= TIMER3_PRESCALER_VAL;
}

void inc_counter_stop (void){
	//clear the prescaler value to stop the counter
	TCCR3B &= ~TIMER3_PRESCALER_VAL;
}




					
static void timer1_init (void)
{     
	//16Bit timer
	OCR1A   =  (F_CPU / F_INTERRUPTS) - 1;	// compare value: 1/15000 of CPU frequency
	TCCR1B  = (1 << WGM12) | (1 << CS10);   // switch CTC Mode on, set prescaler to 1
	TIMSK1  = 1 << OCIE1A;                  // OCIE1A: Interrupt by timer compare
}
				
				
ISR(TIMER3_COMPA_vect)
{
	//every 100ms..
	PORTC ^= (1 << PORTC2 );
	
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



uint8_t ir_cmd_volup = 16;
uint8_t ir_cmd_voldown = 17;


#define STATE_INIT 0
#define STATE_UART_CMD_REC 1
#define STATE_IR_CMD_REC 2
#define CMD_PROC 3

#define DEBUG_MSG 1

uint8_t CMD_REC_UART = 0;
uint8_t CMD_REC_IR = 0;


uint8_t FSM_STATE = 0;

					
int main(void)
{
	IRMP_DATA   irmp_data;
    irmp_init(); // initialize IRMP
	timer1_init();
	timer3_init();
	
	
	char buf[5];
	
	uart0_init(UART_BAUD_SELECT(57600, F_CPU));
	//uart1_init(UART_BAUD_SELECT(57600, F_CPU));

	_delay_ms(200);		//wait until the boot message of ESP8266 at 74880 baud has passed
	
	
	
	//Pin Configurations
	 DDRC |= (1 << PORTC2); //Direction Control Register (1=Output)
	
	sei();







	while (1)
	{
		if (uart0_getln(uart0_line_buf) == GET_LN_RECEIVED){
			// got a command via UART (WiFi)
			//cmd_parser(uart0_line_buf);
			CMD_REC_UART = 1;
		}
		
		if (irmp_get_data (&irmp_data)){
			// got an IR message
			CMD_REC_IR = 1;
		}
		
		switch( FSM_STATE ) {
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
				#endif
					
					CMD_REC_IR = 0;
				}
				break;
			
			case STATE_UART_CMD_REC:
				//Call cmd parser
				cmd_parser(uart0_line_buf);

				FSM_STATE = CMD_PROC;
				break;
				
			case STATE_IR_CMD_REC:
				//Call cmd parser
				if (irmp_data.command == ir_cmd_volup){
					volup(0, NULL);
					inc_counter_start();
				} 
				else if (irmp_data.command == ir_cmd_voldown){
					voldown(0,NULL);
					inc_counter_stop();
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
		}
		
		
		 //if (irmp_get_data (&irmp_data)){
			 //
			//// got an IR message, do something
			//uart0_puts ("protocol: 0x");
			//itoh (buf, 2, irmp_data.protocol);
			//uart0_puts (buf);
			//
			//uart0_puts ("   address: 0x");
			//itoh (buf, 4, irmp_data.address);
			//uart0_puts (buf);
//
			//uart0_puts ("   command: 0x");
			//itoh (buf, 4, irmp_data.command);
			//uart0_puts (buf);
//
			//uart0_puts ("   flags: 0x");
			//itoh (buf, 2, irmp_data.flags);
			//uart0_puts (buf);
//
			//uart0_puts ("\r\n");
			//
			//
			//if ( irmp_data.command == ir_cmd_volup ){
				//volup(0, NULL);
			//}
			//else if ( irmp_data.command == ir_cmd_voldown) {
				//voldown(0, NULL);
			//}
			//else {
				//uart0_puts("unbelegte taste");
			//}
        //}
	}
}