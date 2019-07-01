/*
 * cmd_action.h
 *
 * Created: 13.04.2019 15:10:43
 *  Author: holzi
 */ 

#include <inttypes.h>

#ifndef CMD_ACTION_H_
#define CMD_ACTION_H_


#define NUM_CMDS			3
#define MAX_CMD_WORD_LEN	15
#define MAX_ARG_LEN			15
#define MAX_NUM_ARG			2
#define ECHO_EN				1


typedef struct
{
	uint8_t arg_cnt;
	void (*cmd_fun_ptr)( uint8_t, char*[] );
	char cmd_word[MAX_CMD_WORD_LEN];
} command;

typedef command *command_ptr;


extern command cmd_set[NUM_CMDS];


void volup(uint8_t argc, char *argv[]);
void voldown(uint8_t argc, char *argv[]);
void setvolume(uint8_t argc, char *argv[]);


uint8_t chk_adc_range(uint16_t);

extern uint8_t inc_cnt_stat;
extern uint8_t motor_stat;
extern volatile uint16_t adc_val;

extern void error_led(uint8_t);

#endif /* CMD_ACTION_H_ */