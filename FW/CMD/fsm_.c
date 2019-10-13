/*
 * fsm.c
 *
 * Created: 08.07.2019 22:31:15
 *  Author: holzi
 */ 

#include "../volctrl.h"
#include "../UART/uart.h"
#include "cmdparser.h"
#include "cmd.h"
#include "../IMRP/irmp.h"
#include <inttypes.h>
#include <util/atomic.h>
#include "stdlib.h"

uint8_t ir_cmd_volup = 16;
uint8_t ir_cmd_voldown = 17;

static char * itoh (char * buf, uint8_t digits, uint16_t number)
{
	for (buf[digits] = 0; digits--; number >>= 4)
	{
		buf[digits] = "0123456789ABCDEF"[number & 0x0F];
	}
	return buf;
}

void fsm (void)
{
	int tmp_err = 0;
	uint8_t tmp;
	char buf[10];
	
	uint16_t adc_val_fsm = 0;
	
	  ATOMIC_BLOCK(ATOMIC_FORCEON){
		adc_val_fsm =   adc_val;
	  }
	
	
	switch ( FSM_STATE ) {
		case STATE_INIT:
		//Init State, do nothing (waiting for commands)
			
		//Wait for UART-commands
		if (CMD_REC_UART) {
			//Call CMD parser
			cmd_parser(uart0_line_buf);
			CMD_REC_UART = 0;
		}

		if (CMD_REC_IR){
			#if DEBUG_MSG
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
			uart0_puts("\r\n");
			#endif
				
			//volup() and voldown() will handle the FSM_STATE varialbe
			if (irmp_data.command == ir_cmd_volup){
				volup(0, NULL);
			}
			else if (irmp_data.command == ir_cmd_voldown){
				voldown(0,NULL);
			}
			CMD_REC_IR = 0;
		}
		break;
			
		case STATE_VOLUP:
		//Check if the Motor is at the upper (right) limit
		if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_HI){
			//Motor potentiometer is at right limit
			#if DEBUG_MSG
			uart0_puts("Motor @ upper lim.!\r\n");
			#endif
			//Do not turn the Motor on
			//Stop timers go to init
			FSM_STATE = STATE_INIT;

			inc_timer_stop();
			set_motor_off();
			break;
		}
			
		if (inc_timer_stat == FALSE){
			//Timer not running
			inc_timer_start();	//Start increment timer
				
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
			#if DEBUG_MSG
			uart0_puts("Motor @ lower lim.!\r\n");
			#endif
			//Do not turn the motor on
			FSM_STATE = STATE_INIT;
			inc_timer_stop();
			set_motor_off();
			break;
		}
			
		if (inc_timer_stat == FALSE){
			//Timer not running
			inc_timer_start();	//Start increment timer
				
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
		if (CMD_REC_IR){
			if (irmp_data.command == ir_cmd_volup){
				FSM_STATE = STATE_VOLUP;
				CMD_REC_IR = 0;
				break;
			}
			else if (irmp_data.command == ir_cmd_voldown){
				FSM_STATE = STATE_VOLDOWN;
				CMD_REC_IR = 0;
				break;
			}
			//Other IR Command than VOLUP or VOLDOWN
			//No Re-Trigger of inc_conter
			inc_timer_stop();
			set_motor_off();
			FSM_STATE = STATE_INIT;
				
		}

		if (CMD_REC_UART){
			//UART
			tmp = peek_volupdown(uart0_line_buf);
			if (tmp == CMD_IDX_VOLUP){
				FSM_STATE = STATE_VOLUP;
				CMD_REC_UART = 0;
				break;
			}
			else if (tmp == CMD_IDX_VOLDOWN){
				FSM_STATE = STATE_VOLDOWN;
				CMD_REC_UART = 0;
				break;
			}
			//The Received Command was not a volup or voldown command
			//->No retrigger
			inc_timer_stop();
			set_motor_off();
			FSM_STATE = STATE_INIT;
			break;
		}
			
		//Check if the Motor reached the limit
		if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_HI){
			#if DEBUG_MSG
			uart0_puts("Motor @ upper lim.!\r\n");
			#endif
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
		if (CMD_REC_IR){
			if (irmp_data.command == ir_cmd_volup){
				FSM_STATE = STATE_VOLUP;
				CMD_REC_IR = 0;
				break;
			}
			else if (irmp_data.command == ir_cmd_voldown){
				FSM_STATE = STATE_VOLDOWN;
				CMD_REC_IR = 0;
				break;
			}
			//Other IR Command than VOLUP or VOLDOWN
			//No Re-Trigger of inc_conter
			inc_timer_stop();
			set_motor_off();
			FSM_STATE = STATE_INIT;
				
		}
			
		if (CMD_REC_UART){
			//UART
			tmp = peek_volupdown(uart0_line_buf);
			if (tmp == CMD_IDX_VOLUP){
				FSM_STATE = STATE_VOLUP;
				CMD_REC_UART = 0;
				break;
			}
			else if (tmp == CMD_IDX_VOLDOWN){
				FSM_STATE = STATE_VOLDOWN;
				CMD_REC_UART = 0;
				break;
			}
			//The Received Command was not a volup or voldown command
			//->No retrigger
				
			inc_timer_stop();
			set_motor_off();
			FSM_STATE = STATE_INIT;
			break;
		}

		//Check if the Motor reached the limit
		if (chk_adc_range(adc_val_fsm) == ADC_POT_STAT_LO){
			#if DEBUG_MSG
			uart0_puts("Motor @ lower lim.!\r\n");
			#endif
			set_motor_off();
			inc_timer_stop();
			FSM_STATE = STATE_INIT;
		}
		break;
			
		case STATE_SETVOL:
			tmp_err = adc_val_fsm - setvol_targ;
		
			if (abs(tmp_err) < SETVOL_TOL) {
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
			tmp_err = adc_val_fsm - setvol_targ;
		
			//Motor passed the correct value
			if ( ((tmp_err > 0) && (get_motor_stat() != MOTOR_STAT_CCW)) ||
			((tmp_err < 0) && (get_motor_stat() != MOTOR_STAT_CW))) {
				set_motor_off();
				uart0_puts("Volume search error!\r\n");
				FSM_STATE = STATE_INIT;
				error_led(TRUE);
			}
			
			if (abs(tmp_err) < SETVOL_TOL) {
				set_motor_off();
				FSM_STATE = STATE_INIT;
				break;
			}
			
			if (CMD_REC_IR){
				if ( (irmp_data.command == ir_cmd_volup) ||  (irmp_data.command == ir_cmd_voldown) ){
					//Dismiss command
					CMD_REC_IR = 0;
				}
				//Other IR Command than VOLUP or VOLDOWN
				//Stop motor go to init and process the cmd
				//inc_timer_stop();
				set_motor_off();
				FSM_STATE = STATE_INIT;
				break;
			}

			if (CMD_REC_UART){
				//UART
				tmp = peek_volupdown(uart0_line_buf);
				if (tmp == CMD_IDX_VOLUP || tmp == CMD_IDX_VOLDOWN){
					//Dismiss command
					CMD_REC_UART = 0;
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