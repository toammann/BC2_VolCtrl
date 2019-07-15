/*
 * cmd_action.c
 *
 * Created: 13.04.2019 15:10:54
 *  Author: holzi
 */ 

#include "../volctrl.h"
#include "cmd.h"
#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
#include "../UART/uart.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <stdlib.h>
#include "avr/eeprom.h"

const uint16_t poti_log_curve[] PROGMEM = 
	{	10,  11,  11,  12,  13,  15,  17,
		21,  24,  25,  28,  30,  37,  48,
		59,  69,  79,  90, 102, 114, 125,
		136, 147, 156, 170, 186, 191, 196,
		207, 220, 232, 242, 256, 279, 308,
		341, 383, 438, 500, 561, 622, 683,
		743, 803, 861, 919, 974, 1009, 1020,
		1023, 1023};
		
		
char * itoh (char * buf, uint8_t digits, uint16_t number)
{
	for (buf[digits] = 0; digits--; number >>= 4)
	{
		buf[digits] = "0123456789ABCDEF"[number & 0x0F];
	}
	return buf;
}

void getadcval(uint8_t argc, char *argv[]){
	
	char buf[11];

	uart0_puts_p(PSTR("ADC Value: "));

	ATOMIC_BLOCK(ATOMIC_FORCEON){
		  uart0_puts(itoa(adc_val, buf, 10));
	}
	uart0_puts_p(PSTR("\r\n"));
}

void inc_timer_start (void){
	if (inc_timer_stat == FALSE){
		//Reset Timer count register
		TCNT3 = 0;
		
		//Set the prescaler value to start the counter
		TCCR3B |= TIMER3_PRESCALER_VAL;
		
		//Set timer status flag
		inc_timer_stat = TRUE;
	}
}

void inc_timer_stop (void){
	//clear the prescaler value to stop the counter
	TCCR3B &= ~TIMER3_PRESCALER_VAL;
	inc_timer_stat = FALSE;
}

void inc_timer_rst (void){
	//Reset Timer count register
	TCNT3 = 0;
}

uint8_t get_motor_stat(void){
	uint8_t pin_motor_cw = (PIND & (1 << PIN_MOTOR_CW));
	uint8_t pin_motor_ccw = (PIND & (1 << PIN_MOTOR_CCW));
	
	uint8_t motor_stat = ((pin_motor_cw >> PIN_MOTOR_CW) << 1) | (pin_motor_ccw >> PIN_MOTOR_CCW);
	
	switch (motor_stat){
		case 0b00:
			return MOTOR_STAT_OFF;
			break;
		case 0b10:
			return MOTOR_STAT_CW;
			break;
		case 0b01:
			return MOTOR_STAT_CCW;
			break;
		default: 
			//ERROR
			error_led(TRUE);
			break;
	}
	return -1;
}

void set_motor_off (void){
	//Set both motor ctrl pins to low
	PORTD &=  ~(1 << PIN_MOTOR_CW);
	PORTD &=  ~(1 << PIN_MOTOR_CCW);
	_delay_ms(MOTOR_OFF_DELAY_MS);
}

uint8_t chk_adc_range(uint16_t val)
{
	if (val <= ADC_POT_LO_TH ){
		//Motor potentiometer left limit
		return ADC_POT_STAT_LO;
	} else if (val >= ADC_POT_HI_TH){
		//Motor potentiometer right limit
		return ADC_POT_STAT_HI;
	}
	else {
		//Motor not at limit
		return ADC_POT_STAT_OK;
	}
}

void set_motor_ccw (void){
	
	switch (get_motor_stat()){
		case MOTOR_STAT_OFF:
			//Motor was off -> turn on in ccw direction
			PORTD &=  ~(1 << PIN_MOTOR_CW);		//0
			PORTD |=  (1 << PIN_MOTOR_CCW);		//1
			break;
			
		case MOTOR_STAT_CCW:
			//Motor is already turning in CCW direction
			//do noting...
			break;

		case MOTOR_STAT_CW:
			//Motor is turning CW -> Turn off Motor
			set_motor_off();
			inc_timer_stop();
			_delay_ms(MOTOR_OFF_DELAY_MS);
			break;

		default: 
			error_led(TRUE); //Error
			break;
	}
}

void set_motor_cw (void){
	
	switch (get_motor_stat()){
		case MOTOR_STAT_OFF:
			//Motor was off -> turn on in cw direction
			PORTD |= (1 << PIN_MOTOR_CW);		//1
			PORTD &=  ~(1 << PIN_MOTOR_CCW);	//0
			break;
		
		case MOTOR_STAT_CCW:
			//Motor is turning CCW -> Turn off Motor
			set_motor_off();
			inc_timer_stop();
			_delay_ms(MOTOR_OFF_DELAY_MS);
			break;
			
		case MOTOR_STAT_CW:
			//Motor is already turning in CW direction
			//do noting...
			break;
		default: break;
	}
}

void volup(uint8_t argc, char *argv[]){
	
	if (argc > cmd_set[CMD_IDX_VOLUP].arg_cnt ){
		uart0_puts_p(PSTR("Volup does not expect a argument!\r\n"));
		return;
	}
	
	//Broadcast a notification via UART
	uart0_puts_p(PSTR("volup detected\r\n"));
	
	//Change FSM_STATE
	FSM_STATE = STATE_VOLUP;
	
}

void voldown(uint8_t argc, char *argv[]){
	//Broadcast a notification via UART
	
	if (argc > cmd_set[CMD_IDX_VOLDOWN].arg_cnt ){
		uart0_puts_p(PSTR("voldown does not expect a argument!\r\n"));
		return;
	}
	
	uart0_puts_p(PSTR("voldown detected\r\n"));
	
	//Change FSM_STATE
	FSM_STATE = STATE_VOLDOWN;
}

void setvolume(uint8_t argc, char *argv[]){
	
	if (argc > cmd_set[CMD_IDX_SETVOL].arg_cnt){
		uart0_puts_p(PSTR("Invalid Argument count!\r\n"));
		return;
	}

	uart0_puts_p(PSTR("setvol detected\r\n"));

	int idx;
	char buffer[5];

	#if DEBUG_MSG
		uart0_puts_p(PSTR("argc: "));
		uart0_puts(itoa(argc, buffer, 10));
		uart0_puts_p(PSTR("\r\n"));
		
		for (int i=0; i < argc; i++)
		{
			uart0_puts_p(PSTR("argv: "));
			uart0_puts(argv[i]);
			uart0_puts_p(PSTR("\r\n"));
		}
	#endif
	
	//Get integer from argument vector (string)
	idx = atoi( argv[0] );
	
	if ( (idx > 100) || (idx < 0) ){
		uart0_puts_p(PSTR("Argument out of range!\r\n"));
		//error_led(TRUE);
		return;
	}
	
	//Result gets cropped if it not a integer
	//->ne next smaller value is taken. z.B. 99/2=49.5 -> 49
	idx = idx/2;

	//To accsess data from program memory
	//Adress the data as normal -> take the address &()
	//use pgm_read marko
	setvol_targ = pgm_read_word( &(poti_log_curve[idx]) );
	
	//Switch to STATE_SETVOL FSM State
	FSM_STATE = STATE_SETVOL;
	
	#if DEBUG_MSG
		uart0_puts_p(PSTR("Target ADC value: "));
		uart0_puts(itoa(setvol_targ, buffer, 10));
		uart0_puts_p(PSTR("\r\n"));
	#endif
}

void error_led (uint8_t status){
	if ( status ){
		//Turn ERROR LED on
		PORTB |= (1 << ERROR_LED);
	}
	else {
		//Turn ERROR LED off
		PORTB &= ~(1 << ERROR_LED);
	}
}

void regrem(uint8_t argc, char *argv[]){
	
	//check if everything is valid
	
	//Arg 0:			Description
	//Arg 1:			CMD Word
	//Arg 2 ... end :	CMD Arguments
	
	ir_key ir_key_tmp;
	uint8_t ani_pt_cnt = 0;
	uint8_t ani_line_cnt = 0;
	uint16_t timeout_cnt = 0;
	const uint8_t waitloop_iter_time = 10; //ms
	uint8_t tmp = 0;
	char desc[MAX_ARG_LEN];
	
	//Check if there is space for more keys
	if (ir_keyset_len > IR_KEY_MAX_NUM){
		uart0_puts_p(PSTR("The maximum numer of keys to register is reached!\r\n"));
		return;
	}
	
	//Copy description to temporary variable
	strcpy(desc, argv[0]);
	
	//Get the command Index from the first argument (command word)
	for (int i = 0; i < NUM_CMDS; i++)
	{
		//search for the input cmd string in available commands
		if ( strcmp( argv[1], cmd_set[i].cmd_word ) == 0){
			//cmd string matches a command
			tmp = 1;
			ir_key_tmp.cmd_idx = i;
			break;
		}
	}
	
	if (tmp == 0){
		//no valid command was found
		uart0_puts_p(PSTR("regrem: You tried to register a unknown command\r\n"));
		ir_key_tmp.cmd_idx = 0xFF;
		return;
	}
	
	//Check if the number of arguments is correnct
	if ( (argc - 2) != cmd_set[ir_key_tmp.cmd_idx].arg_cnt ){
		uart0_puts_p(PSTR("regrem: Invalid number of arguments for cmd to register\r\n"));
		ir_key_tmp.cmd_idx = 0xFF;
		return;
	}
	

	ir_key_tmp.arg_str[0] = 0;
	//Append all remaining arguments to arg_str
	if ( argc > 2 ) {
		//strcpy(ir_key_tmp.arg_str," ");

		
		for (int i = 2; i < argc; i++){
			
			strcat(ir_key_tmp.arg_str, " ");
			strcat(ir_key_tmp.arg_str, argv[i]);
			
		}
	}
	
	//Wait for of a user input of a new ir-keypress
	uart0_puts_p(PSTR("Press the desired key on the ir-remote\r\n"));
	while (!irmp_get_data (&irmp_data)){
		//Got no IR Message...
		//Print Wait Animation
		if (ani_pt_cnt > 8){
			
			if (ani_line_cnt > 15){
				uart0_putc('\r');
				ani_line_cnt = 0;
			}
			uart0_putc('.');
			ani_line_cnt++;
			ani_pt_cnt = 0;
		}
			
		//Increment Wait time
		_delay_ms(waitloop_iter_time);
			
		ani_pt_cnt++;
		timeout_cnt++;

		if ( timeout_cnt*waitloop_iter_time >= IR_KEY_REG_TIMEOUT*1000){
			uart0_puts_p(PSTR("\r\nTimeout!\r\n"));
			return;
		}
	}
	uart0_puts_p(PSTR("\r\n"));

	//We have a valid Keypress!
	uart0_puts_p(PSTR("Keypress registered\r\n"));

	//Fill the Key Data
	ir_key_tmp.key_data.ir_prot   = irmp_data.protocol;
	ir_key_tmp.key_data.ir_addr = irmp_data.address;
	ir_key_tmp.key_data.ir_cmd = irmp_data.command;

	//Copy the data to the ir_keyset array
	ir_keyset[ir_keyset_len] = ir_key_tmp;
	ir_keyset_len++;

	uart0_puts_p(PSTR("Write to EEPROM...\r\n"));

	//Update EEPROM
	eeprom_update_block( (void*) desc , (void*) &(eeprom_ir_key_desc[ir_keyset_len - 1][0]), sizeof(desc));
	eeprom_update_block( (void*) ir_keyset , (void*) eeprom_ir_keyset, sizeof(ir_keyset));
	eeprom_update_byte( &eeprom_ir_keyset_len, ir_keyset_len);

	//Print Info
	uart0_puts_p(PSTR("Key register successful!\r\n"));
	
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
}

void delrem(uint8_t argc, char *argv[]){
	
	if (argc > cmd_set[CMD_IDX_DELREM].arg_cnt){
		uart0_puts_p(PSTR("Invalid Argument count!\r\n"));
		return;
	}
	
	if (ir_keyset_len < 1){
		uart0_puts_p(PSTR("No remote key registered. Nothing to delete!\r\n"));
		return;
	}
	
	uart0_puts_p(PSTR("Delete Key with index: "));
	uart0_puts(argv[0]);
	uart0_puts_p(PSTR("\r\n"));
	
	//Get integer from argument vector (string)
	uint8_t idx;
	char desc_tmp[MAX_ARG_LEN];
	idx = atoi( argv[0] );
	
	//Check if the received idx was valid
	if ( (idx + 1) > ir_keyset_len ){
		uart0_puts_p(PSTR("Index out of range!\r\n"));
		return;
	}
	
	//Update the Keyset array
	for (int i = 0; i < IR_KEY_MAX_NUM; i++){
		if (i == (IR_KEY_MAX_NUM -1)){
			//last element is to be deleted
			//noting to copy
			break;
		}
		else if (i >= idx){
			//Copy data one index up
			ir_keyset[i] = ir_keyset[i+1];
			
			//Copy description data one index up
			eeprom_read_block( (void*) desc_tmp , (void*) &(eeprom_ir_key_desc[i+1]), sizeof(eeprom_ir_key_desc[0]));
			eeprom_write_block( (void*) desc_tmp , (void*) &(eeprom_ir_key_desc[i]), sizeof(eeprom_ir_key_desc[0]));
		}
	}
	
	//One element was deleted
	ir_keyset_len--;

	//Update EEPROM
	eeprom_update_block( (void*) ir_keyset , (void*) eeprom_ir_keyset, sizeof(ir_keyset));
	eeprom_update_byte( &eeprom_ir_keyset_len, ir_keyset_len);
	
	uart0_puts_p(PSTR("Deleted!\r\n"));
}

void showrem(uint8_t argc, char *argv[]){
	
	if (argc > cmd_set[CMD_IDX_SHOWREM].arg_cnt){
		uart0_puts_p(PSTR("Invalid Argument count!\r\n"));
		return;
	}
	
	uart0_puts_p(PSTR(" IDX | PROTOCOL | IR_ADDR | IR_CMD  | CMD          | DESCRIPTION\r\n"));

	char desc_tmp[MAX_ARG_LEN];
	char buf[10];
	//char whitesp[5];
	
	const uint8_t column_width_idx = 4;
	const uint8_t column_width_prot = 9;
	const uint8_t column_width_ir_cmd = 6;
	const uint8_t column_width_ir_addr = 6;
	const uint8_t column_width_cmd = 13;
	
	for (int i = 0; i < ir_keyset_len ; i++){

		//INDEX
		uart0_putc(' ');
		uart0_puts(itoa(i, buf, 10));
		for (int i = 0; i < (column_width_idx - strlen(buf)); i++){
			uart0_putc(' ');
		}
		uart0_putc('|');
		
		//PROTOCOL
		uart0_putc(' ');
		strcpy_P(buf, (char*) pgm_read_word(&(irmp_protocol_names[ir_keyset[i].key_data.ir_prot])));
		uart0_puts(buf);
		for (int i = 0; i < (column_width_prot - strlen(buf)); i++){
			uart0_putc(' ');
		}
		uart0_putc('|');
		
		//IR_ADDR
		uart0_puts_p(PSTR(" 0x"));
		
		uart0_puts(itoh(buf, 4, ir_keyset[i].key_data.ir_addr));
		for (int i = 0; i < (column_width_ir_addr - strlen(buf)); i++){
			uart0_putc(' ');
		}
		uart0_putc('|');
		
		
		//IR_CMD
		uart0_puts_p(PSTR(" 0x"));
		
		uart0_puts(itoh(buf, 4, ir_keyset[i].key_data.ir_cmd));
		for (int i = 0; i < (column_width_ir_cmd - strlen(buf)); i++){
			uart0_putc(' ');
		}
		uart0_putc('|');
		
		//CMD
		uart0_putc(' ');
		strcpy(buf, cmd_set[ir_keyset[i].cmd_idx].cmd_word);
		strcat(buf, ir_keyset[i].arg_str);
		uart0_puts(buf);
		for (int i = 0; i < (column_width_cmd - strlen(buf)); i++){
			uart0_putc(' ');
		}
		uart0_putc('|');
		
		//DESCRIPTION
		uart0_putc(' ');
		eeprom_read_block( (void*) desc_tmp , (void*) &(eeprom_ir_key_desc[i]), sizeof(eeprom_ir_key_desc[0]));
		uart0_puts(desc_tmp);
		uart0_puts_p(PSTR("\r\n"));
	}
}

void set5vled(uint8_t argc, char *argv[]){
	if (argc > cmd_set[CMD_IDX_SET5VLED].arg_cnt){
		uart0_puts_p(PSTR("Invalid Argument count!\r\n"));
		return;
	}
	
	if ( *argv[0] == '1'){
		//Turn LED on
		PORTE |= (1 << PWR_5V_LED);
		eeprom_update_byte(&eeprom_pwr_5v_led, 1);
		uart0_puts_p(PSTR("5V LED ON!\r\n"));
		return;
		
	} else if (*argv[0] == '0'){
		//Turn LED off
		PORTE &= ~(1 << PWR_5V_LED);
		eeprom_update_byte(&eeprom_pwr_5v_led, 0);
		uart0_puts_p(PSTR("5V LED OFF!\r\n"));
		return;
	}
	//Invalid argument
	uart0_puts_p(PSTR("Invalid Argument \r\n"));
}

void set3v3led(uint8_t argc, char *argv[]){
	
	if (argc > cmd_set[CMD_IDX_SET3V3LED].arg_cnt){
		uart0_puts_p(PSTR("Invalid Argument count!\r\n"));
		return;
	}

	if (  *argv[0] == '1'){
		//Turn LED on
		PORTD |= (1 << PWR_3V3_LED);
		eeprom_update_byte(&eeprom_pwr_3v3_led, 1);
		uart0_puts_p(PSTR("3V3 LED ON!\r\n"));
		return;
		} 
		else if ( *argv[0] == '0'){
		//Turn LED off
		PORTD &= ~(1 << PWR_3V3_LED);
		eeprom_update_byte(&eeprom_pwr_3v3_led, 0);
		uart0_puts_p(PSTR("3V3 LED OFF!\r\n"));
		return;
	}
	//Invalid argument
	uart0_puts_p(PSTR("Invalid Argument \r\n"));
}

	

