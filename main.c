/*
 * VolCtrl_FW.c
 *
 * Created: 07.03.2019 23:49:44
 * Author : holzi
 */ 

#include "volctrl.h"
#include <avr/io.h>
#include "avr/interrupt.h"
#include "./UART/uart.h"
#include "./CMD/cmdparser.h"
#include <inttypes.h>
//#include <string.h>
//#include <util/delay.h>
#include "./IMRP/irmp.h"
#include "./CMD/cmd.h"

//#include "stdlib.h"

//GLOBAL VARIABLES
volatile uint8_t inc_timer_stat = 0;
volatile uint8_t FSM_STATE = 0;
volatile uint16_t adc_val = 0;
uint16_t setvol_targ = 0;

IRMP_DATA   irmp_data;

uint8_t CMD_REC_UART = 0;
uint8_t CMD_REC_IR = 0;

command cmd_set[NUM_CMDS] ={{0, &volup, "volup"},			//CMD_IDX 0, consider define in cmd.h
							{0, &voldown, "voldown"},		//CMD_IDX 1, consider define in cmd.h
							{1, &setvolume, "setvol"},		//CMD_IDX 2, consider define in cmd.h
							{0, &getadcval, "getadcval"}};	//CMD_IDX 3, consider define in cmd.h


//uint8_t ir_cmd_volup = 16;
//uint8_t ir_cmd_voldown = 17;

//timer 0 für fehler überwachung

//Volume increment counter
void timer3_init (void){
	//16Bit Timer
	//TC1 Control Register B
	//Mode 4 - CTC (clear timer on compare)
	TCCR3B =  (1 << WGM32);
	
	//Do not set the prescaler -> timer is not started!

	//Output Compare Register 3 A Low and High byte 
	//Sets the counter value at which the interrupt gets executed
	OCR3A = (uint16_t) TIMER_COMP_VAL(TIMER3_PRESCALER, INC_DURATION); //16Bit value is correct addressed automatically
	//OCR3A = 23437; //for 500ms
	
	//Counter 3 Interrupt Mask Register
	//Set OCIE3A Flag: Timer/Counter 3, Output Compare A Match Interrupt Enable
 	TIMSK3 = (1 << OCIE3A);
}

//IRMP Timer		
static void timer1_init (void)
{     
	//16Bit timer
	OCR1A   =  (F_CPU / F_INTERRUPTS) - 1;	// compare value: 1/15000 of CPU frequency
	TCCR1B  = (1 << WGM12) | (1 << CS10);   // switch CTC Mode on, set prescaler to 1
	TIMSK1  = 1 << OCIE1A;                  // OCIE1A: Interrupt by timer compare
}		

//Potentiometer position adc			
void adc0_init(){
	//AREF, Internal Vref turned OFF
	//ADC MUX = 0 (ADC0)
	ADMUX = 0x00;
	
	//Enable ADC
	//ADC Interrupt Enable
	//Division Factor = 128
	//
	ADCSRA = ( 1 << ADEN) | (1 << ADIE) |
	( 1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2) |
	( 1 << ADATE);
	//ADCSRB = 0; //Free Running mode
	
	//Start conversion (first result is invalid)
	ADCSRA |= (1 << ADSC);
	
	//Disable digital input buffer
	DIDR0 = (1 << ADC0D);
}
				
// Timer1 output compare A interrupt service routine, called every 1/15000 sec	
// IRMP Timer	
ISR(TIMER1_COMPA_vect)
{
	(void) irmp_ISR();	//Call IRMP ISR
}

//Timer 3 Volume increment timer
ISR(TIMER3_COMPA_vect)
{
	//every INC_DURATION ms
	//clear the prescaler value to stop the counter
	inc_timer_stop();
}

ISR(ADC_vect){
	//Read ADC Value
	adc_val = ADC;
}
				
int main(void)
{
	
    irmp_init();			// initialize IRMP
	timer1_init();			//IRMP Timer
	timer3_init();			//Volume increment timer
	adc0_init();			//Potentiometer position adc
	
	uart0_init(UART_BAUD_SELECT(BAUDRATE, F_CPU));
	//uart1_init(UART_BAUD_SELECT(BAUDRATE, F_CPU));

	_delay_ms(200);			//wait until the boot message of ESP8266 at 74880 baud has passed
	
	uart0_puts("Hello here is the VolCtrl FW ");
	uart0_puts(FW_VERSION);
	uart0_puts("\r\n");
	
	//Pin Configurations
	//Direction Control Register (1=output, 0=input)
	DDRB = (1 << ERROR_LED); 
	DDRC = (1 << PORTC2);
	DDRD = (1 << PIN_MOTOR_CW) | (1 << PIN_MOTOR_CCW);

	//Pullup Config
	PORTD = 0;				//Deactivate pullups

	error_led(FALSE);		//Turn off Error LED

	sei();					//Activate Interrupts

	while (1)
	{
		//uart0_puts(itoa(adc_val, buf, 10));
		//uart0_puts("\r\n");
		
		if (uart0_getln(uart0_line_buf) == GET_LN_RECEIVED){
			//got a command via UART (WiFi)
			//cmd_parser(uart0_line_buf);
			CMD_REC_UART = TRUE;
		}
		
		if (irmp_get_data (&irmp_data)){
			// got an IR message
			CMD_REC_IR = TRUE;
		}


		fsm();

	}
}