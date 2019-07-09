/*
 * cmd_action.h
 *
 * Created: 13.04.2019 15:10:43
 *  Author: holzi
 */ 

#include <inttypes.h>
#include "../IMRP/irmp.h"

#ifndef CMD_ACTION_H_
#define CMD_ACTION_H_


#define NUM_CMDS			4
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
extern volatile uint8_t FSM_STATE;
extern volatile uint8_t inc_timer_stat;	//Increment counter status
extern uint16_t setvol_targ;
extern volatile uint16_t adc_val;
extern IRMP_DATA   irmp_data;
extern uint8_t CMD_REC_UART;
extern uint8_t CMD_REC_IR;

void volup(uint8_t argc, char *argv[]);
void voldown(uint8_t argc, char *argv[]);
void setvolume(uint8_t argc, char *argv[]);
void getadcval(uint8_t argc, char *argv[]);


void inc_timer_stop (void);
void set_motor_off (void);
void set_motor_cw (void);
void set_motor_ccw (void);
void inc_timer_start (void);
void inc_timer_rst (void);
uint8_t get_motor_stat(void);
uint8_t chk_adc_range(uint16_t);
void error_led(uint8_t);

void fsm(void);

#endif /* CMD_ACTION_H_ */