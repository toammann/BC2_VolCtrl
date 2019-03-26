/*
 * VolCtrl_FW.c
 *
 * Created: 07.03.2019 23:49:44
 * Author : holzi
 */ 
#define F_CPU 20000000UL  // Systemtakt in Hz - Definition als unsigned long beachten

#define NUM_CMDS	3
#define MAX_CMD_WORD_LEN 15
#define MAX_ARG_LEN 15
#define MAX_NUM_ARG 2
#define ECHO_EN 1

#include <avr/io.h>
#include "avr/interrupt.h"
#include "./UART/uart.h"
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

uint16_t tmp;


void volup(uint8_t argc, char *argv[]){
	uart0_puts("volup detected\r\n");
}

void voldown(uint8_t argc, char *argv[]){
	uart0_puts("voldown detected\r\n");
}

void setVolume(uint8_t argc, char *argv[]){
		
		
	char buffer[5];
	
	uart0_puts("setvolume detected\rn");
	
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

//void (*fun_ptr)(void) = &volup; 


typedef struct
{
	uint8_t arg_cnt;
	void (*cmd_fun_ptr)( uint8_t, char*[] );
	char cmd_word[MAX_CMD_WORD_LEN];
} command;

typedef command *command_ptr;

command cmd_set[NUM_CMDS] = {{0, &volup, "volup"},
							 {0, &voldown, "voldown"},
							 {1, &setVolume, "setvolume"}};
								 
uint8_t cmd_parser(char* cmd){
	
	if (ECHO_EN) {
		uart0_puts(cmd);
		uart0_puts("\r\n");
	}
	
	command_ptr detc_cmd = NULL;
	char delim[] = " ,";		// " " and ","
	
	uint8_t argc;				//Recognized argument count
	uint8_t err;				//Error-Flag
	char *argv[MAX_NUM_ARG];	//argument vector containing pointers to strings
	uint8_t tmp_strlen;

	//convert input string to lowercase
	//command interpreter should be case insensitive
	strlwr(cmd);
	
	//Receive the first token
	char *token = strtok(cmd, delim);
	
	//The first token is the command word
	for (int i = 0; i < NUM_CMDS; i++)
	{
		//search for the input cmd string in available commands 
		if ( strcmp( token, cmd_set[i].cmd_word ) == 0)
		{
			//cmd string matches a command
			detc_cmd = &cmd_set[i];
		}
	}
	
	if (detc_cmd == NULL){
		//No cmd string found
		uart0_puts("Not valid command!\r\n");
		return -1;
	}
	
	//all other tokens are arguments
	//Collect all arguments in cmd
	argc = 0;
	err = 0;
	
	token = strtok(NULL, delim);
	while(token != NULL)
	{
		//ignore empty tokens (eg. 10, 11) the " " would be a empty token
		if( !(strcmp(token, "") == 0) ){

			//Check number of arguments
			if ((argc >= detc_cmd->arg_cnt) || (argc >= MAX_NUM_ARG)){
				uart0_puts("Too many arguments!\r\n");
				err = 1;
				break;
			}
			
			//Check argument string length
			tmp_strlen = strlen(token); // strlen is not including '\0'
			if ( tmp_strlen + 1 >= MAX_ARG_LEN ){
				uart0_puts("Max arg string length exceeded!\r\n");
				err = 1;
				break;
			}
			
			//allocate memory for argument string
			argv[argc] = (char *) malloc(tmp_strlen  + 1); 
			
			if (argv[argc] == NULL){
				//Memory allocation failed
				uart0_puts("Memory allocation failed!\r\n");
				err = 1;
				break;
			}
	
			//copy the token to the argument vector
			strcpy(argv[argc] , token);
			
			//increase argument counter			
			argc++;
		}
		//Fetch the next token to process		
		token = strtok(NULL, delim);
	}
	
	//all arguments parsed, check if the correct number of arguments was found
	//do not print a error message if the err flag is already set
	if ( (argc != detc_cmd->arg_cnt) && (err == 0) ){
		uart0_puts("Incorrect number of Arguments!\r\n");
		err=1;
	}
	
	if (!err){
		//If all went fine call the command function and pass the arguments
		detc_cmd->cmd_fun_ptr(argc, argv);
	}

	//free allocated memory
	for (int i = 0; i < argc; i++){
		free(argv[i]);
	}
	
	if (err) return 1;
	else return 0;
};





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
		if (uart0_getln(uart0_line_buf)){
			cmd_parser(uart0_line_buf);
		}
		
	}

}

