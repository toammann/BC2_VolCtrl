/*
 * pin_cfg.h
 *
 * Created: 30.05.2019 16:37:34
 *  Author: holzi
 */ 


#ifndef VOLCTRL_H_
#define VOLCTRL_H_

#define F_CPU 12000000UL  // Systemtakt in Hz - Definition als unsigned long beachten

#ifndef TRUE
#  define FALSE   0
#  define TRUE    1
#endif

#define DEBUG_MSG 1

#define TIMER3_PRESCALER_VAL	(1 << CS32) //TIMER1 Prescaler=256
#define TIMER3_PRESCALER		256


#define PIN_MOTOR_CW			PORTD3
#define PIN_MOTOR_CCW			PORTD2
#define ERROR_LED				PORTB5

#define MOTOR_STAT_OFF			0
#define MOTOR_STAT_CW			1
#define MOTOR_STAT_CCW			2
#define MOTOR_OFF_DELAY_MS		100	//Motor off delay if the rotation direction is changed


#define ADC_POT_HI_TH 800
#define ADC_POT_LO_TH 200
#define ADC_POT_STAT_LO 1
#define ADC_POT_STAT_HI 2
#define ADC_POT_STAT_OK 0

#define INC_DURATION			500


#define TIMER_COMP_VAL(prescaler,duration) ((float) duration*F_CPU/prescaler/1000)
//#define TIMER_COMP_VAL(prescaler,duration) (23437)


#endif /*VOLCTRL_H_ */