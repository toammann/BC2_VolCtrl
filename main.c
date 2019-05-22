/*
 * VolCtrl_FW.c
 *
 * Created: 07.03.2019 23:49:44
 * Author : holzi
 */ 

#include <avr/io.h>
#include "avr/interrupt.h"
#include "./UART/uart.h"
#include "./CMD/cmdparser.h"
#include <inttypes.h>
#include <string.h>
#include <util/delay.h>


#define F_CPU 12000000UL  // Systemtakt in Hz - Definition als unsigned long beachten
#include "./IMRP/irmp.h"

					
					
static void timer1_init (void)
{                                                             // ATmegaXX:
	OCR1A   =  (F_CPU / F_INTERRUPTS) - 1;                                  // compare value: 1/15000 of CPU frequency
	TCCR1B  = (1 << WGM12) | (1 << CS10);                                   // switch CTC Mode on, set prescaler to 1
	

	TIMSK1  = 1 << OCIE1A;                                                  // OCIE1A: Interrupt by timer compare
}
					

				
ISR(TIMER1_COMPA_vect)                                                             // Timer1 output compare A interrupt service routine, called every 1/15000 sec
{
	(void) irmp_ISR();                                                        // call irmp ISR
	// call other timer interrupt routines...
}
	
			

static char *
itoh (char * buf, uint8_t digits, uint16_t number)
{
	for (buf[digits] = 0; digits--; number >>= 4)
	{
		buf[digits] = "0123456789ABCDEF"[number & 0x0F];
	}
	return buf;
}



uint8_t ir_cmd_volup = 16;
uint8_t ir_cmd_voldown = 17;


					
int main(void)
{
	IRMP_DATA   irmp_data;
    irmp_init(); // initialize IRMP
	timer1_init();
	char buf[5];
	
	uart0_init(UART_BAUD_SELECT(57600, F_CPU));
	//uart1_init(UART_BAUD_SELECT(57600, F_CPU));

	_delay_ms(200);		//wait until the boot message of ESP8266 at 74880 baud has passed
	
	sei();

	while (1)
	{
		if (uart0_getln(uart0_line_buf) == GET_LN_RECEIVED){
			// got a command via uart (WiFi), do something
			cmd_parser(uart0_line_buf);
		}
		
		
		 if (irmp_get_data (&irmp_data)){
			 
			// got an IR message, do something
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

			uart0_puts ("\r\n");
			
			
			if ( irmp_data.command == ir_cmd_volup ){
				volup(0, NULL);
			}
			else if ( irmp_data.command == ir_cmd_voldown) {
				voldown(0, NULL);
			}
			else {
				uart0_puts("unbelegte taste");
			}
        }
	}
}