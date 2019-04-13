/*
 * cmd_parser.c
 *
 * Created: 13.04.2019 14:51:44
 *  Author: holzi
 */ 


#include "cmdparser.h"
#include "cmd.h"
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>

char uart0_line_buf[LINE_BUF_SIZE];		//Line buffer used by uart0_getln



/*************************************************************************
Function: cmd_parser()
Purpose:  Parses the string in cmd for arguments and valid commands
          Calls the matching command function with arguments
Input:    pointer to a char array
Returns:  0x00 no error occoured
		  0x01 error occoured
**************************************************************************/			 
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
		uart0_puts("Unknown command!\r\n");
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


/*************************************************************************
Function: uart0_getln()
Purpose:  reads a line from UART buffer (delimiter); '\b' and DEL=127 delete the most
		  recent chr; '\n' characters are ignored
		  The implementation is non blocking
Input:    pointer to a line buffer
Returns:  0x00 no bytes available
		  0x01 one line was read successfully
		  0x02 UART transmit Error occurred
**************************************************************************/
uint16_t uart0_getln(char* uart0_line_buf)
{
	if (uart0_available() > 0){
		//Bytes received
		static uint8_t uart0_line_buf_len = 0;
		
		uint16_t rec_val;		//received value
		char rec_c;				//received character
		
		rec_val = uart0_getc();
		rec_c = (char)rec_val;	//lower 8 bit
		
		//Check for receive errors
		if ( uart0_errchk(rec_val) ){
			return uart0_errchk(rec_val);
		}

		if ( rec_c == LINE_DELIMITER ){
			//EOL reached
			if (uart0_line_buf_len != 0){
				//reset buffer index
				uart0_line_buf_len = 0;
			}
			else{
				//first character was a delimiter -> set terminator to first buffer index
				//(empty string)
				uart0_line_buf[uart0_line_buf_len] = 0;
			}
			return 0x01;
		}
		else {
			//EOL not reached 
			
			//Handle backspace and "DEL" (=127)
			if ( rec_c == '\b' || rec_c == 127 ){
				//delete the most recent character
				//Prevent buf len from overflow
				if (uart0_line_buf_len > 0) uart0_line_buf_len--;
				uart0_line_buf[uart0_line_buf_len] = 0;
			}
			else if (rec_c == '\n'){
				//Ignore Characters. E.g. '\n' if the EOL is "\r\n" in case of a telnet connection
				
			}
			else {
				//-> store to buffer
				if(uart0_line_buf_len < LINE_BUF_SIZE){
					uart0_line_buf[uart0_line_buf_len++] = rec_c;
					uart0_line_buf[uart0_line_buf_len] = 0; // append the null terminator
				}
				else{
					//buffer full -> print error message
					uart0_puts("Line length exceeds buffer!");
				}
			}
		}
	}
	return 0x00;
}


/*************************************************************************
Function: uart0_errchk()
Purpose:  checks the error bytes and transmits an error message via UART in 
		  case of an error
Input:    None
Returns:  boolean false if no error was found; true if an error occured
**************************************************************************/
uint16_t uart0_errchk(uint16_t rec_val){
	
	if (rec_val & UART_FRAME_ERROR ){
		uart0_puts("UART_FRAME_ERROR occurred!");
		return UART_FRAME_ERROR;
	}
	else if (rec_val & UART_OVERRUN_ERROR){
		uart0_puts("UART_OVERRUN_ERROR occurred!");
		return UART_OVERRUN_ERROR;
	}
	else if (rec_val & UART_BUFFER_OVERFLOW){
		uart0_puts("UART_BUFFER_OVERFLOW occurred!");
		return UART_BUFFER_OVERFLOW;
	}
	else if (rec_val & UART_NO_DATA){
		uart0_puts("UART_NO_DATA occurred!");
		return UART_NO_DATA;
	}
	return 0;
}