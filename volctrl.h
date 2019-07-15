/*
 * pin_cfg.h
 *
 * Created: 30.05.2019 16:37:34
 *  Author: holzi
 */ 


#ifndef VOLCTRL_H_
#define VOLCTRL_H_

#define FW_VERSION	"v1.0"
#define F_CPU		8000000UL  // Systemtakt in Hz - Definition als unsigned long beachten
#define BAUDRATE	57600


#define DEBUG_MSG			1

#define NUM_CMDS			9
#define MAX_CMD_WORD_LEN	10
#define MAX_ARG_LEN			10
#define MAX_NUM_ARG			3

#define IR_KEY_REG_TIMEOUT	5	 //s
#define IR_KEY_MAX_NUM		13

#define IR_KEYSET_EEPROM_ADDR 512

#define TIMER3_PRESCALER_VAL	(1 << CS32) //TIMER1 Prescaler=256
#define TIMER3_PRESCALER		256

#define PIN_MOTOR_CW			PORTD3
#define PIN_MOTOR_CCW			PORTD2
#define ERROR_LED				PORTB5
#define PWR_3V3_LED				PORTD5
#define PWR_5V_LED				PORTE1

#define MOTOR_STAT_OFF			0
#define MOTOR_STAT_CW			1
#define MOTOR_STAT_CCW			2
#define MOTOR_OFF_DELAY_MS		100	//Motor off delay if the rotation direction is changed

#define ADC_POT_HI_TH			1023
#define ADC_POT_LO_TH			10
#define ADC_POT_STAT_LO			1
#define ADC_POT_STAT_HI			2
#define ADC_POT_STAT_OK			0

#define INC_DURATION			150 //1400ms max.
#define SETVOL_TOL				1

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


#define STATE_INIT				0
#define STATE_VOLUP				1
#define STATE_VOLDOWN			2
#define STATE_SETVOL			3
#define STATE_SETVOL_ACT		4
#define STATE_VOLUP_ACT			5
#define STATE_VOLDOWN_ACT		6

#define TIMER_COMP_VAL(prescaler,duration) ((float) duration*F_CPU/prescaler/1000)



#ifndef TRUE
#  define FALSE   0
#  define TRUE    1
#endif


#endif /*VOLCTRL_H_ */