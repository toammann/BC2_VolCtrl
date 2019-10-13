/*
 * fsm.c
 *
 * Implements a finite-state-machine which controlles
 * the ALPS motor potentiometer all "not volume control command" gets
 * executed right away. The volume control commands "setvol, volup, voldown" changes
 * the states in the fsm. For a visual representation of the fsm refer to github
 *
 * Created: 08.07.2019 22:31:15
 *  Author: Tobias Ammann
 */ 

#include "../volctrl.h"
#include "../UART/uart.h"
#include "cmdparser.h"
#include "cmd.h"
#include "../IMRP/irmp.h"
#include <inttypes.h>
#include <util/atomic.h>
#include "stdlib.h"

static int adc_run_dist;
static uint8_t tmp;
static uint16_t adc_val_fsm = 0;
static char tmp_cmd_str[MAX_CMD_WORD_LEN + MAX_ARG_LEN*MAX_NUM_ARG];

static ir_key_data ir_key_tmp;
static uint8_t cmd_idx_tmp;
static uint8_t keyset_idx_tmp;
static uint8_t cmd_idx_tmp_stat;


static uint8_t CMD_REC_UART = 0;
static uint8_t CMD_REC_IR = 0;

//Retuns the CMD index of the received IR Command
void get_ir_cmd_idx(IRMP_DATA irmp_tmp_dat, uint8_t* cmd_idx_stat, uint8_t* cmd_idx, uint8_t* keyset_idx){
		
	ir_key_tmp.ir_addr =  irmp_data.address;
	ir_key_tmp.ir_cmd = irmp_data.command;
	ir_key_tmp.ir_prot = irmp_data.protocol;
			
	for (uint8_t i = 0; i < ir_keyset_len; i++){
				
		if ( memcmp( (void*) &(ir_key_tmp), (void*) &(ir_keyset[i].key_data), sizeof(ir_key_data)) == 0){
			//Valid key found!
			*cmd_idx =  ir_keyset[i].cmd_idx;
			*keyset_idx = i;
			*cmd_idx_stat = TRUE;
			return;
		}
	}
	//No key found!
	*cmd_idx_stat = FALSE;
	*cmd_idx = 0xFF;
}

//Checks for a new command while a volup or voldown cmd is active
void check_for_new_cmds_volupdown_act(void){
	char line_buf_tmp[LINE_BUF_SIZE];
	
	if (CMD_REC_IR){
		//IR-Commands
		get_ir_cmd_idx(irmp_data, &cmd_idx_tmp_stat, &cmd_idx_tmp, &keyset_idx_tmp);
		
		if (cmd_idx_tmp_stat){
			if (cmd_idx_tmp == CMD_IDX_VOLUP){
				FSM_STATE = STATE_VOLUP;
				CMD_REC_IR = 0;
				return;
			}
			else if (cmd_idx_tmp == CMD_IDX_VOLDOWN){
				FSM_STATE = STATE_VOLDOWN;
				CMD_REC_IR = 0;
				return;
			}
		}
		
		//The Received Command was not a volup or voldown command
		//->No retrigger
		inc_timer_stop();
		set_motor_off();
		FSM_STATE = STATE_INIT;
	}
		
	if (CMD_REC_UART){
		//UART
		//make a copy of uart line buffer, peek_volctrl modifies the string
		strcpy(line_buf_tmp, uart0_line_buf);
		tmp = peek_volctrl(line_buf_tmp);
		if (tmp == CMD_IDX_VOLUP){
			FSM_STATE = STATE_VOLUP;
			CMD_REC_UART = 0;
			return;
		}
		else if (tmp == CMD_IDX_VOLDOWN){
			FSM_STATE = STATE_VOLDOWN;
			CMD_REC_UART = 0;
			return;
		}
		//The Received Command was not a volup or voldown command
		//->No retrigger
		inc_timer_stop();
		set_motor_off();
		FSM_STATE = STATE_INIT;
	}
}

//FINITE-STATE-MACHINE
void fsm (void){

	//adc_run_dist = 0;
	char line_buf_tmp[LINE_BUF_SIZE];
	
	//Check uart for new messages
	if (uart0_getln(uart0_line_buf) == GET_LN_RECEIVED){
		//got a command via UART (WiFi)
		CMD_REC_UART = TRUE;
	}

	//Check IRMP for new messages
	if (irmp_get_data (&irmp_data)){
		// got an IR message
		CMD_REC_IR = TRUE;
	}

	//Get current adc value for poti position reading
	ATOMIC_BLOCK(ATOMIC_FORCEON){
		adc_val_fsm =   adc_val;
	  }
	
	
	switch ( FSM_STATE ) {
		case STATE_INIT:
		//Init State, do nothing (wait for commands)
			
		//Wait for UART-commands
		if (CMD_REC_UART) {
			//Call CMD parser
			cmd_parser(uart0_line_buf);
			CMD_REC_UART = 0;
		}

		if (CMD_REC_IR){
			#if DEBUG_MSG
				char buf[10];
				uart0_puts_p(PSTR("protocol: 0x"));
				itoh (buf, 2, irmp_data.protocol);
				uart0_puts(buf);
				
				uart0_puts_p(PSTR("   address: 0x"));
				itoh (buf, 4, irmp_data.address);
				uart0_puts(buf);
				
				uart0_puts_p(PSTR("   command: 0x"));
				itoh (buf, 4, irmp_data.command);
				uart0_puts(buf);
				
				uart0_puts_p(PSTR("   flags: 0x"));
				itoh (buf, 2, irmp_data.flags);
				uart0_puts(buf);
				uart0_puts_p(PSTR("\r\n"));
			#endif
				
			if (irmp_data.flags == 1) {
				//Ignore repetitive keypresses
				//Only the first button press should trigger a
				//Command it is possible that a button was pressed 
				//to stop the setvol command. The second ir comannd (with flag == 1)
				//should not trigger a cmd as this is not intended by the human
				CMD_REC_IR = 0;
				break;
			}

			//Check if the received IR-Command matches a key in the keyset
			ir_key_tmp.ir_addr =  irmp_data.address;
			ir_key_tmp.ir_cmd = irmp_data.command;
			ir_key_tmp.ir_prot = irmp_data.protocol;
			
			for (int i = 0; i < ir_keyset_len; i++){
				
				if ( memcmp( (void*) &(ir_key_tmp), (void*) &(ir_keyset[i].key_data), sizeof(ir_key_data)) == 0){
					//Valid key found!

					//Build cmd string
					strcpy(tmp_cmd_str, cmd_set[ir_keyset[i].cmd_idx].cmd_word);
					strcat(tmp_cmd_str, ir_keyset[i].arg_str);
					
					//Pass the data to cmd parser
					cmd_parser(tmp_cmd_str);
					//THe commands will handle the FSM_STATE Variable
				}
			}
			CMD_REC_IR = 0;
		}
		break;
			
		case STATE_VOLUP:
		//Check if the Motor is at the upper (right) limit
		if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_HI){
			//Motor potentiometer is at right limit
			uart0_puts_p(PSTR("Motor @ upper lim.!\r\n"));
			
			//Do not turn the Motor on
			//Stop timers go to init
			FSM_STATE = STATE_INIT;

			inc_timer_stop();
			set_motor_off();
			break;
		}
			
		if (inc_timer_stat == FALSE){
			//Timer not running
			inc_timer_start();
				
			//Start motor if it is not already running
			if ( get_motor_stat() == MOTOR_STAT_OFF ){
				set_motor_cw();		//Start motor in cw direction (volup)
				} else {
				//ERROR: running motor without a timer is not allowed. Turn on error indicator LED
				error_led(TRUE);
				set_motor_off();
			}
		}
		else {
			//Timer already running (e.g from a previous volup or voldown cmd)
			inc_timer_rst(); // restart timer
				
			//Rotation direction is unknown, here -> handled in set_motor_cw()
			set_motor_cw();
		}
		//Go to volume up active sate
		FSM_STATE = STATE_VOLUP_ACT;
		break;

		case STATE_VOLDOWN:
			//Check if the Motor is at the lower (left) limit
			if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_LO){
				//Motor potentiometer is at right limit
				uart0_puts_p(PSTR("Motor @ lower lim.!\r\n"));
				
				//Do not turn the motor on
				FSM_STATE = STATE_INIT;
				inc_timer_stop();
				set_motor_off();
				break;
			}
			
			if (inc_timer_stat == FALSE){
				//Timer not running
				inc_timer_start();
				
				//Start motor if it is not already running
				if ( get_motor_stat() == MOTOR_STAT_OFF ){
					set_motor_ccw();		//Start motor in ccw direction (voldown)
					} else {
					//ERROR: running motor without a timer is not allowed turn on error indicator LED
					error_led(TRUE);
					set_motor_off();
				}
			}
			else {
				//Timer already running (e.g from a previous volup cmd)
				inc_timer_rst(); // restart timer
				
				//Rotation direction is unknown, here -> handled in set_motor_cw()
				set_motor_ccw();
			}
			FSM_STATE = STATE_VOLDOWN_ACT;
		break;
			
		case STATE_VOLUP_ACT:
			//Timer stopped (time elapsed)
			if ( !inc_timer_stat){
				//INC-Duration exceeded
				set_motor_off();
				FSM_STATE = STATE_INIT;
				break;
			}
			
			//New Command Received
			check_for_new_cmds_volupdown_act();
			
			//Check if the Motor reached the limit
			if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_HI){
				
				uart0_puts_p(PSTR("Motor @ upper lim.!\r\n"));
				
				set_motor_off();
				inc_timer_stop();
				FSM_STATE = STATE_INIT;
			}
			break;
			
			case STATE_VOLDOWN_ACT:
			//Timer stopped (time elapsed)
			if ( !inc_timer_stat){
				//INC-Duration exceeded
				set_motor_off();
				FSM_STATE = STATE_INIT;
				break;
			}

			//New command received
			check_for_new_cmds_volupdown_act();

			//Check if the Motor reached the limit
			if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_LO){
				
				uart0_puts_p(PSTR("Motor @ lower lim.!\r\n"));

				set_motor_off();
				inc_timer_stop();
				FSM_STATE = STATE_INIT;
			}
		break;
			
		case STATE_SETVOL:
			adc_run_dist = adc_val_fsm - setvol_targ;
		
			if (abs(adc_run_dist) < SETVOL_TOL) {
					//Noting to do
					set_motor_off();
					FSM_STATE = STATE_INIT;
					break;
				}
			
			//Get correct rotation direction
			if (adc_val_fsm < setvol_targ) {
				//We have to rotate CW
				set_motor_cw();
			}
			else{
				// we have to rote CCW
				set_motor_ccw();
			}
			FSM_STATE = STATE_SETVOL_ACT;
			break;
			
		case STATE_SETVOL_ACT:
			adc_run_dist = adc_val_fsm - setvol_targ;
		
			//Motor passed the correct value
			if ( ((adc_run_dist > 0) && (get_motor_stat() != MOTOR_STAT_CCW)) ||
			((adc_run_dist < 0) && (get_motor_stat() != MOTOR_STAT_CW))) {
				set_motor_off();
				uart0_puts_p(PSTR("Volume search error!\r\n"));
				FSM_STATE = STATE_INIT;
				error_led(TRUE);
			}
			
			if (abs(adc_run_dist) < SETVOL_TOL) {
				set_motor_off();
				FSM_STATE = STATE_INIT;
				break;
			}
			

			if (CMD_REC_IR){
				get_ir_cmd_idx(irmp_data, &cmd_idx_tmp_stat, &cmd_idx_tmp, &keyset_idx_tmp);
				
				if (cmd_idx_tmp_stat){
					if (cmd_idx_tmp == CMD_IDX_VOLUP || cmd_idx_tmp == CMD_IDX_VOLDOWN){
						//Ignore command
						CMD_REC_IR = 0;
					}
					else if (cmd_idx_tmp == CMD_IDX_SETVOL){
						//Retrigger of setvolume, possibly with a new target value
						//execute the complete setvol cmd!
						strcpy(tmp_cmd_str, cmd_set[cmd_idx_tmp].cmd_word);
						strcat(tmp_cmd_str, ir_keyset[keyset_idx_tmp].arg_str);
						
						//Pass the data to cmd parser
						cmd_parser(tmp_cmd_str);
						CMD_REC_IR = 0;
						break;
					}
				}
				
				//Other IR Command than VOLUP, VOLDOWN or SETVOl
				//Stop motor go to init and process the cmd
				set_motor_off();
				FSM_STATE = STATE_INIT;
				//CMD_REC_IR = 0;
				break;
				
			}

			if (CMD_REC_UART){
				//UART
				//make a copy of uart line buffer, peek_volctrl modifies the string
				strcpy(line_buf_tmp, uart0_line_buf);
				cmd_idx_tmp = peek_volctrl(line_buf_tmp);
				
				if (cmd_idx_tmp == CMD_IDX_VOLUP || cmd_idx_tmp == CMD_IDX_VOLDOWN){
					//Dismiss command
					CMD_REC_UART = 0;
				} 
				else if (cmd_idx_tmp == CMD_IDX_SETVOL){
					//Execute Setvol CMD -> retrigger
					cmd_parser(uart0_line_buf);
					CMD_REC_UART = 0;
					break;
				}
				
				//Other UART Command than VOLUP or VOLDOWN
				//Stop motor go to init and process the cmd
				//inc_timer_stop();
				set_motor_off();
				FSM_STATE = STATE_INIT;
				break;
		}
		default: break;
	}
}