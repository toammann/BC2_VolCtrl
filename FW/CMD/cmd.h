/*
 * cmd_action.h
 *
 * Created: 13.04.2019 15:10:43
 *  Author: holzi
 */ 

#include <inttypes.h>
#include "../IMRP/irmp.h"
#include "avr/eeprom.h"
#ifndef CMD_ACTION_H_
#define CMD_ACTION_H_



typedef struct
{
	uint8_t arg_cnt;
	void (*cmd_fun_ptr)( uint8_t, char*[] );
	char cmd_word[MAX_CMD_WORD_LEN];
} command;

typedef command *command_ptr;


typedef struct
{
	uint8_t  ir_prot;	//IR Protokoll
	uint16_t ir_addr;	//IR Address
	uint16_t ir_cmd;	//IR Command
} ir_key_data;

typedef struct
{
	ir_key_data key_data;
	uint8_t cmd_idx;	//cmd_index of cmd_set
	char arg_str[(MAX_ARG_LEN -1)*MAX_NUM_ARG]; //regrem will the the command with the most arguments, the first arg is the cmd word -> -1
} ir_key;

extern command cmd_set[NUM_CMDS];
extern ir_key ir_keyset[IR_KEY_MAX_NUM];

extern uint8_t EEMEM eeprom_ir_keyset_len;
extern uint8_t EEMEM eeprom_pwr_5v_led;
extern uint8_t EEMEM eeprom_pwr_3v3_led;
extern ir_key EEMEM eeprom_ir_keyset[IR_KEY_MAX_NUM];
extern char EEMEM eeprom_ir_key_desc[IR_KEY_MAX_NUM][MAX_ARG_LEN];

extern uint8_t ir_keyset_len;
extern volatile uint8_t FSM_STATE;
extern volatile uint8_t inc_timer_stat;	//Increment counter status
extern uint16_t setvol_targ;
extern volatile uint16_t adc_val;
extern IRMP_DATA irmp_data;
extern uint8_t CMD_REC_UART;
extern uint8_t CMD_REC_IR;

void volup(uint8_t argc, char *argv[]);
void voldown(uint8_t argc, char *argv[]);
void setvolume(uint8_t argc, char *argv[]);
void getadcval(uint8_t argc, char *argv[]);
void regrem(uint8_t argc, char *argv[]);
void delrem(uint8_t argc, char *argv[]);
void showrem(uint8_t argc, char *argv[]);

char * itoh (char * buf, uint8_t digits, uint16_t number);
void inc_timer_stop (void);
void set_motor_off (void);
void set_motor_cw (void);
void set_motor_ccw (void);
void inc_timer_start (void);
void inc_timer_rst (void);
uint8_t get_motor_stat(void);
uint8_t chk_adc_range(uint16_t);
void error_led(uint8_t);
void set5vled(uint8_t argc, char *argv[]);
void set3v3led(uint8_t argc, char *argv[]);

void fsm(void);

#endif /* CMD_ACTION_H_ */