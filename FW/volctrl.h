/*
 * volctrl.h
 *
 * Created: 30.05.2019 16:37:34
 *  Author: holzi
 */ 

#ifndef VOLCTRL_H_
#define VOLCTRL_H_

#define FW_VERSION				"v1.0"		//Firmware version
#define F_CPU					8000000UL	//System Clock in in Hz
#define BAUDRATE				57600		//Baudrate setting

#define DEBUG_MSG				0	//Toggles Debug Messages on or off

//DEFINES FOR THE CMD SET
#define NUM_CMDS				11	//Numer of commands
#define MAX_CMD_WORD_LEN		10  //Maximum cmd word length
#define MAX_ARG_LEN				10	//Maximum arg word length
#define MAX_NUM_ARG				3	//Maximum number of arguments

//DEFINES FOR THE REGKEY CMD
#define IR_KEY_REG_TIMEOUT		5	 //s
#define IR_KEY_MAX_NUM			13	 //Maxumum number of allowed keys to store in eeprom (larger number needs more ram)

#define MOTOR_OFF_DELAY_MS		100  //ms, Motor off delay if the rotation direction is changed

//ADC POTENTIOMETER HIGH/LOW THRESHOLD
#define ADC_POT_HI_TH			1023
#define ADC_POT_LO_TH			9

//TOLERANCE OF THE SETVOL CMD in LSBs
#define SETVOL_TOL				1

//EEPROM DEFAULT INC DURATION IN MS
#define EEPROM_INC_DURATION		150 //1400ms max.

//STATUS BYTES FOR THE HIGH/LOW BOUNDARY
#define ADC_POT_STAT_LO			1
#define ADC_POT_STAT_HI			2
#define ADC_POT_STAT_OK			0

#define TIMER3_PRESCALER_VAL	(1 << CS32) //TIMER1 Prescaler=256
#define TIMER3_PRESCALER		256

//MACRO TO CALCULATE THE TIMER VALUE FOR INC_DURATION
#define TIMER_COMP_VAL(prescaler,duration) ((float) duration*F_CPU/prescaler/1000)

// PIN DEFINES
#define PIN_MOTOR_CW			PORTD3
#define PIN_MOTOR_CCW			PORTD2
#define ERROR_LED				PORTB5
#define PWR_3V3_LED				PORTD5
#define PWR_5V_LED				PORTE1

//MOTOR STATUS DEFINES
#define MOTOR_STAT_OFF			0
#define MOTOR_STAT_CW			1
#define MOTOR_STAT_CCW			2

//CMD INDEXES
//has to be unique
#define CMD_IDX_VOLUP			0
#define CMD_IDX_VOLDOWN			1
#define CMD_IDX_SETVOL			2
#define CMD_IDX_GETADC			3
#define CMD_IDX_REGREM			4
#define CMD_IDX_DELREM			5
#define CMD_IDX_SHOWREM			6
#define CMD_IDX_SET5VLED		7
#define CMD_IDX_SET3V3LED		8
#define CMD_IDX_SETINCDUR		9
#define CMD_IDX_GETINCDUR		10

//FSM STATES 
#define STATE_INIT				0
#define STATE_VOLUP				1
#define STATE_VOLDOWN			2
#define STATE_SETVOL			3
#define STATE_SETVOL_ACT		4
#define STATE_VOLUP_ACT			5
#define STATE_VOLDOWN_ACT		6


#ifndef TRUE
#  define FALSE   0
#  define TRUE    1
#endif


#endif /*VOLCTRL_H_ */