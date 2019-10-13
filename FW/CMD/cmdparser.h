/*
 * cmd_parser.h
 * 
 * A string parser for UART Commands. It searched for command words in cmd_set and 
 * checks the for the correct number of arguments.
 *
 * Created: 13.04.2019 14:52:07
 *  Author: Tobias Ammann
 */ 

#include <inttypes.h>
#include "../UART/uart.h"
#include "../CMD/cmd.h"

#ifndef CMD_PARSER_H_
#define CMD_PARSER_H_

#define  GET_LN_NO_BYTES	1
#define  GET_LN_REC_ERR		2
#define  GET_LN_RECEIVED	0

#define  LINE_DELIMITER		'\r'	
#define  LINE_BUF_SIZE		40		
#define	 CMD_SEPARATORS		" ,"	//For strtok

extern char uart0_line_buf[];

/**
 *  @brief   Parses the string in cmd for arguments and valid commands
 *           calls the matching command function with arguments
 *  @return  0 - if no error occured; 1 if a error occured
 */
uint8_t cmd_parser(char* cmd);


/**
 *  @brief   Reads a line from receive buffer
 *  @return  String containing the read line
 */
uint16_t uart0_getln(char* uart0_line_buf);

/**
 *  @brief   Checks the upper 16 bits of rec_val for error flags
 *  @return  16Bit UART error code
 */
uint16_t uart0_errchk(uint16_t rec_val);

/**
 *  @brief   peeks if the string in buffer is volume control command
 *			 (volup, voldown, setvol) which affects the fsm. The function
 *		     uses strtok and therefore modifies the buffer
 *  @return  CMD_INX of the vol ctrl cmd if a valid cmd was found, 0xFF otherwise
 */
uint8_t peek_volctrl(char* buffer);

#endif /* CMD_PARSER_H_ */