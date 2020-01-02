/*--------------------------------------------------------------------------------------------------------
 * main.c - Firmware:BC2VolCtrl PCB, handles IR and UART messages and controls a ALPS motor potentiometer
 *
 * Copyright (c) 2019 Tobias Ammann 
 *
 * Firmware for the BC2VolCtrl PCB. It decodes IR-Commands (using the IRMP library) and 
 * string commands via UART. After parsing a command (cmdparser.c) the corresponding 
 * function is called (cmd.c). A ALPS motor potentiometer which is driven by a h-bride circuit is 
 * controlled by a finite state machine which handles the all motor and timer start and stop conditions.
 *
 * Special Thanks to the authors of:
 * UART Library (Andy Gock)
 * IRMP Library (Frank Meyer) 
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and 
 * to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions 
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *---------------------------------------------------------------------------------------------------------
 */

#include "volctrl.h"
#include <avr/io.h>
#include <inttypes.h>
#include "avr/interrupt.h"
#include "./UART/uart.h"
#include "./IMRP/irmp.h"
#include "./CMD/cmd.h"
#include "avr/eeprom.h"

//GLOBAL VARIABLES (INTERRUPT)
volatile uint8_t inc_timer_stat = 0;
volatile uint8_t FSM_STATE = 0;
volatile uint16_t adc_val = 0;

//Global IRMP DATA STRUCT
IRMP_DATA irmp_data;

//MISC GLOBAL VARIABLES
uint16_t setvol_targ = 0;
uint8_t ir_keyset_len = 0;
ir_key ir_keyset[IR_KEY_MAX_NUM];
uint16_t inc_dur;


//COMMAND SET: (ALL SUPPORTED COMMANDS)
//CONSIDER CMD INDEXES IN volctrl.h
command cmd_set[NUM_CMDS] = {{0, &volup,	 "volup"},			
							 {0, &voldown,	 "voldown"},		
							 {1, &setvolume, "setvol"},		
							 {0, &getadcval, "getadcval"},
							 {2, &regrem,	 "regrem"},
							 {1, &delrem,	 "delrem"},
							 {0, &showrem,   "showrem"},
							 {1, &set5vled,  "set5vled"},
							 {1, &set3v3led, "set3v3led"}, 
							 {1, &setincdur, "setincdur"},
							 {0, &getincdur, "getincdur"}};
								 
//EEEPROM DEFLAUT VALUES
uint8_t  EEMEM eeprom_ir_keyset_len = 0;
uint8_t  EEMEM eeprom_pwr_5v_led = 1;
uint8_t  EEMEM eeprom_pwr_3v3_led = 1;
ir_key   EEMEM eeprom_ir_keyset[IR_KEY_MAX_NUM];
char     EEMEM eeprom_ir_key_desc[IR_KEY_MAX_NUM][MAX_ARG_LEN];
uint16_t EEMEM eeprom_inc_dur = EEPROM_INC_DURATION;


/*------------------------------------------------------------------------------------------------------
 * TIMER INITIALIZATION
 *------------------------------------------------------------------------------------------------------*/

// TIMER 3: Volume increment counter
static void timer3_init (void){
	//16Bit Timer
	//TC1 Control Register B
	//Mode 4 - CTC (clear timer on compare)
	TCCR3B =  (1 << WGM32);
	
	//Do not set the prescaler -> timer is not started!

	//Output Compare Register 3 A Low and High byte 
	//Sets the counter value at which the interrupt gets executed
	OCR3A = (uint16_t) TIMER_COMP_VAL(TIMER3_PRESCALER, inc_dur); //16Bit value is correct addressed automatically
	
	//Counter 3 Interrupt Mask Register
	//Set OCIE3A Flag: Timer/Counter 3, Output Compare A Match Interrupt Enable
 	TIMSK3 = (1 << OCIE3A);
}

// TIMER 3 Volume increment interrupt service routine, called every inc_duration
ISR(TIMER3_COMPA_vect)
{
	//every INC_DURATION ms
	//clear the prescaler value to stop the counter
	inc_timer_stop();
}

// TIMER 1: IRMP Timer		
static void timer1_init (void)
{     
	//16Bit timer
	OCR1A   =  (F_CPU / F_INTERRUPTS) - 1;	// compare value: 1/15000 of CPU frequency
	TCCR1B  = (1 << WGM12) | (1 << CS10);   // switch CTC Mode on, set prescaler to 1
	TIMSK1  = 1 << OCIE1A;                  // OCIE1A: Interrupt by timer compare
}		

// TIMER :1 IRMP Interrupt service routine, called every 1/15000 sec
ISR(TIMER1_COMPA_vect)
{
	(void) irmp_ISR();	//Call IRMP ISR
}

/*------------------------------------------------------------------------------------------------------
 * ADC INITIALIZATION
 *------------------------------------------------------------------------------------------------------*/

//ADC0 INIT: Potentiometer position		
void adc0_init(){
	//AREF, Internal Vref turned OFF
	//ADC MUX = 0 (ADC0)
	ADMUX = 0x00;
	
	//Enable ADC
	//ADC Interrupt Enable
	//Division Factor = 128
	ADCSRA = ( 1 << ADEN) | (1 << ADIE) |
	( 1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2) |
	( 1 << ADATE);
	//ADCSRB = 0; //Free Running mode
	
	//Start conversion (first result is invalid)
	ADCSRA |= (1 << ADSC);
	
	//Disable digital input buffer
	DIDR0 = (1 << ADC0D);
}
				
// ADC 0
ISR(ADC_vect){
	//Read ADC Value
	adc_val = ADC;
}
				
/*------------------------------------------------------------------------------------------------------
 * MAIN LOOP
 *------------------------------------------------------------------------------------------------------*/
int main(void)
{
	//Read Data from EEPROM
	ir_keyset_len = eeprom_read_byte(&eeprom_ir_keyset_len);
	eeprom_read_block( (void*) ir_keyset , (void*) eeprom_ir_keyset, sizeof(eeprom_ir_keyset));
	inc_dur = eeprom_read_word(&eeprom_inc_dur);
	
	//Pin Configurations
	//Direction Control Register (1=output, 0=input)
	DDRB = (1 << ERROR_LED);
	DDRC = (1 << PORTC2);
	DDRE = (1 << PWR_5V_LED);
	DDRD = (1 << PIN_MOTOR_CW) | (1 << PIN_MOTOR_CCW) | (1 << PWR_3V3_LED);;
	
	//Pullup Config
	PORTC = ~(1 << PORTC0);					//Deactivate Pullup at PC0
	PORTB = (1 << PORTB1) || (1 << PORTB3);	//Activate Pullup at AVR_TXD1_MOSI0, PB1 defined level for U5 buffer
	//PORTE = 0;			
	
	//INIT error LED
	error_led(FALSE);		//Turn off Error LED

	//Init 5V Power LED
	if (eeprom_read_byte(&eeprom_pwr_5v_led)){
		//Turn LED on
		PORTE |= (1 << PWR_5V_LED);
		} else {
		//Turn LED off
		PORTE &= ~(1 << PWR_5V_LED);
	}

	//Init 3V3 Power LED
	if (eeprom_read_byte(&eeprom_pwr_3v3_led)){
		//Turn LED on
		PORTD |= (1 << PWR_3V3_LED);
		} else {
		//Turn LED off
		PORTD &= ~(1 << PWR_3V3_LED);
	}
	
	//Initialize Timers and ADC
    irmp_init();			//initialize IRMP library
	timer1_init();			//IRMP Timer
	timer3_init();			//Volume increment timer
	adc0_init();			//Potentiometer position adc
	
	//INIT UART
	uart0_init(UART_BAUD_SELECT(BAUDRATE, F_CPU));		//INIT UART0
	//uart1_init(UART_BAUD_SELECT(BAUDRATE, F_CPU));
	
	//Send a firmware identifier via UART0
	uart0_puts_p(PSTR("BC2 VolCtrl FW: "));
	uart0_puts_p(PSTR(FW_VERSION));
	uart0_puts_p(PSTR("\r\n"));
		
	_delay_ms(500);			//wait until the boot message of ESP8266 at 74880 baud has passed
	sei();					//Activate Interrupts

	while (1)
	{
		fsm();
	}
}