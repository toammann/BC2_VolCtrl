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

uint16_t tmp;

					
int main(void)
{
	sei();
		
	uart0_init(UART_BAUD_SELECT(9600, F_CPU));
	uart1_init(UART_BAUD_SELECT(9600, F_CPU));
	
    /* Replace with your application code 
    while (1) 
    {		
		if (uart0_getln(uart0_line_buf) == 0x01){
			uart0_puts(uart0_line_buf);
			uart0_putc('\r');
			//uart0_putc('\n');
		}

		if (uart1_available() > 0){
			tmp = uart1_getc();
			uart1_putc(tmp & 0x00FF);
		}
		
	}*/
	
	while (1)
	{
		if (uart0_getln(uart0_line_buf) == GET_LN_RECEIVED){
			cmd_parser(uart0_line_buf);
		}
		
	}

}

