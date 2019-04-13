/*
 * cmd_action.c
 *
 * Created: 13.04.2019 15:10:54
 *  Author: holzi
 */ 


#include "cmd.h"
#include <avr/io.h>
#include <stdlib.h>
#include "../UART/uart.h"


command cmd_set[NUM_CMDS] ={{0, &volup, "volup"},
							{0, &voldown, "voldown"},
							{2, &setVolume, "setvol"}};


void volup(uint8_t argc, char *argv[]){
	uart0_puts("volup detected\r\n");
}

void voldown(uint8_t argc, char *argv[]){
	uart0_puts("voldown detected\r\n");
}

void setVolume(uint8_t argc, char *argv[]){
		
		
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
