/*************************************************************************

	Title:    Interrupt UART library with receive/transmit circular buffers
	Author:   Andy Gock
	Software: AVR-GCC 4.1, AVR Libc 1.4
	Hardware: any AVR with built-in UART, tested on AT90S8515 & ATmega8 at 4 Mhz
	License:  GNU General Public License
	Usage:    see README.md and Doxygen manual

	Based on original library by Peter Fluery, Tim Sharpe, Nicholas Zambetti.

	https://github.com/andygock/avr-uart

	Updated UART library (this one) by Andy Gock
	https://github.com/andygock/avr-uart

	Based on updated UART library (this one) by Tim Sharpe
	http://beaststwo.org/avr-uart/index.shtml

	Based on original library by Peter Fluery
	http://homepage.hispeed.ch/peterfleury/avr-software.html

*************************************************************************/

/*************************************************************************

LICENSE:
	Copyright (C) 2012 Andy Gock
	Copyright (C) 2006 Peter Fleury

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

*************************************************************************/

/************************************************************************
uart_available, uart_flush, uart1_available, and uart1_flush functions
were adapted from the Arduino HardwareSerial.h library by Tim Sharpe on
11 Jan 2009.  The license info for HardwareSerial.h is as follows:

  HardwareSerial.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 23 November 2006 by David A. Mellis
************************************************************************/

/************************************************************************
Changelog for modifications made by Tim Sharpe, starting with the current
  library version on his Web site as of 05/01/2009.

Date        Description
=========================================================================
05/11/2009  Changed all existing UARTx_RECEIVE_INTERRUPT and UARTx_TRANSMIT_INTERRUPT
			macros to use the "_vect" format introduced in AVR-Libc
			v1.4.0.  Had to split the 3290 and 6490 out of their existing
			macro due to an inconsistency in the UART0_RECEIVE_INTERRUPT
			vector name (seems like a typo: USART_RX_vect for the 3290/6490
			vice USART0_RX_vect for the others in the macro).
			Verified all existing macro register names against the device
			header files in AVR-Libc v1.6.6 to catch any inconsistencies.
05/12/2009  Added support for 48P, 88P, 168P, and 328P by adding them to the
			existing 48/88/168 macro.
			Added Arduino-style available() and flush() functions for both
			supported UARTs.  Really wanted to keep them out of the library, so
			that it would be as close as possible to Peter Fleury's original
			library, but has scoping issues accessing internal variables from
			another program.  Go C!
05/13/2009  Changed Interrupt Service Routine label from the old "SIGNAL" to
			the "ISR" format introduced in AVR-Libc v1.4.0.

************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include "uart.h"

/*
 *  constants and macros
 */

/* size of RX/TX buffers */
#define UART_RX0_BUFFER_MASK (UART_RX0_BUFFER_SIZE - 1)
#define UART_RX1_BUFFER_MASK (UART_RX1_BUFFER_SIZE - 1)

#define UART_TX0_BUFFER_MASK (UART_TX0_BUFFER_SIZE - 1)
#define UART_TX1_BUFFER_MASK (UART_TX1_BUFFER_SIZE - 1)

#if (UART_RX0_BUFFER_SIZE & UART_RX0_BUFFER_MASK)
	#error RX0 buffer size is not a power of 2
#endif
#if (UART_TX0_BUFFER_SIZE & UART_TX0_BUFFER_MASK)
	#error TX0 buffer size is not a power of 2
#endif

#if (UART_RX1_BUFFER_SIZE & UART_RX1_BUFFER_MASK)
	#error RX1 buffer size is not a power of 2
#endif
#if (UART_TX1_BUFFER_SIZE & UART_TX1_BUFFER_MASK)
	#error TX1 buffer size is not a power of 2
#endif

//defined(__AVR_ATmega328PB__)
/* ATmega with two USART */
#define ATMEGA_USART0
#define ATMEGA_USART1
#define UART0_RECEIVE_INTERRUPT   USART0_RX_vect
#define UART1_RECEIVE_INTERRUPT   USART1_RX_vect
#define UART0_TRANSMIT_INTERRUPT  USART0_UDRE_vect
#define UART1_TRANSMIT_INTERRUPT  USART1_UDRE_vect
#define UART0_STATUS   UCSR0A
#define UART0_CONTROL  UCSR0B
#define UART0_DATA     UDR0
#define UART0_UDRIE    UDRIE0
#define UART1_STATUS   UCSR1A
#define UART1_CONTROL  UCSR1B
#define UART1_DATA     UDR1
#define UART1_UDRIE    UDRIE1


/*
 *  Module global variables
 */

#if defined(USART0_ENABLED)
	static volatile uint8_t UART_TxBuf[UART_TX0_BUFFER_SIZE];
	static volatile uint8_t UART_RxBuf[UART_RX0_BUFFER_SIZE];

	char uart0_line_buf[LINE_BUF_SIZE];

	#if defined(USART0_LARGE_BUFFER)
		static volatile uint16_t UART_TxHead;
		static volatile uint16_t UART_TxTail;
		static volatile uint16_t UART_RxHead;
		static volatile uint16_t UART_RxTail;
		static volatile uint8_t UART_LastRxError;
	#else
		static volatile uint8_t UART_TxHead;
		static volatile uint8_t UART_TxTail;
		static volatile uint8_t UART_RxHead;
		static volatile uint8_t UART_RxTail;
		static volatile uint8_t UART_LastRxError;
	#endif
#endif

#if defined(USART1_ENABLED)
	static volatile uint8_t UART1_TxBuf[UART_TX1_BUFFER_SIZE];
	static volatile uint8_t UART1_RxBuf[UART_RX1_BUFFER_SIZE];
		
	#if defined(USART1_LARGE_BUFFER)
		static volatile uint16_t UART1_TxHead;
		static volatile uint16_t UART1_TxTail;
		static volatile uint16_t UART1_RxHead;
		static volatile uint16_t UART1_RxTail;
		static volatile uint8_t UART1_LastRxError;
	#else
		static volatile uint8_t UART1_TxHead;
		static volatile uint8_t UART1_TxTail;
		static volatile uint8_t UART1_RxHead;
		static volatile uint8_t UART1_RxTail;
		static volatile uint8_t UART1_LastRxError;
	#endif		
#endif

#if defined(USART0_ENABLED)

ISR(UART0_RECEIVE_INTERRUPT)
/*************************************************************************
Function: UART Receive Complete interrupt
Purpose:  called when the UART has received a character
**************************************************************************/
{
    uint16_t tmphead;
    uint8_t data;
    uint8_t usr;
    uint8_t lastRxError;
 
    /* read UART status register and UART data register */ 
    usr  = UART0_STATUS;
    data = UART0_DATA;
    
    /* */
    lastRxError = (usr & (_BV(FE0)|_BV(DOR0)));		//DOR0 = Data OverRun 0 ;  FE0?Frame Error 0
        
    /* calculate buffer index */ 
    tmphead = (UART_RxHead + 1) & UART_RX0_BUFFER_MASK;
    
    if (tmphead == UART_RxTail) {
        /* error: receive buffer overflow */
        lastRxError = UART_BUFFER_OVERFLOW >> 8;
    } else {
        /* store new index */
        UART_RxHead = tmphead;
        /* store received data in buffer */
        UART_RxBuf[tmphead] = data;
    }
    UART_LastRxError = lastRxError;   
}


ISR(UART0_TRANSMIT_INTERRUPT)
/*************************************************************************
Function: UART Data Register Empty interrupt
Purpose:  called when the UART is ready to transmit the next byte
**************************************************************************/
{
    uint16_t tmptail;

    if (UART_TxHead != UART_TxTail) {
        /* calculate and store new buffer index */
        tmptail = (UART_TxTail + 1) & UART_TX0_BUFFER_MASK;
        UART_TxTail = tmptail;
        /* get one byte from buffer and write it to UART */
        UART0_DATA = UART_TxBuf[tmptail];  /* start transmission */
    } else {
        /* tx buffer empty, disable UDRE interrupt */
        UART0_CONTROL &= ~_BV(UART0_UDRIE);
    }
}


/*************************************************************************
Function: uart0_init()
Purpose:  initialize UART and set baudrate
Input:    baudrate using macro UART_BAUD_SELECT()
Returns:  none
**************************************************************************/
void uart0_init(uint16_t baudrate)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		UART_TxHead = 0;
		UART_TxTail = 0;
		UART_RxHead = 0;
		UART_RxTail = 0;
	}
	
	/* Set baud rate */
	if (baudrate & 0x8000) {
		UART0_STATUS = (1<<U2X0);  //Enable 2x speed
		baudrate &= ~0x8000;
	}
	UBRR0H = (uint8_t)(baudrate>>8);
	UBRR0L = (uint8_t) baudrate;

	/* Enable USART receiver and transmitter and receive complete interrupt */
	UART0_CONTROL = _BV(RXCIE0)|(1<<RXEN0)|(1<<TXEN0);

	/* Set frame format: asynchronous, 8data, no parity, 1stop bit */
	#ifdef URSEL0
		UCSR0C = (1<<URSEL0)|(3<<UCSZ00);
	#else
		UCSR0C = (3<<UCSZ00);
	#endif
	
} /* uart0_init */


/*************************************************************************
Function: uart0_getc()
Purpose:  return byte from ringbuffer
Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error
**************************************************************************/
uint16_t uart0_getc(void)
{
	uint16_t tmptail;
	uint8_t data;

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if (UART_RxHead == UART_RxTail) {
			return UART_NO_DATA;   /* no data available */
		}
	}
	
	/* calculate / store buffer index */
	tmptail = (UART_RxTail + 1) & UART_RX0_BUFFER_MASK;
	
	UART_RxTail = tmptail;
	
	/* get data from receive buffer */
	data = UART_RxBuf[tmptail];

	return (UART_LastRxError << 8) + data;

} /* uart0_getc */

/*************************************************************************
Function: uart0_peek()
Purpose:  Returns the next byte (character) of incoming UART data without
          removing it from the ring buffer. That is, successive calls to
		  uartN_peek() will return the same character, as will the next
		  call to uartN_getc()
Returns:  lower byte:  next byte in ring buffer
          higher byte: last receive error
**************************************************************************/
uint16_t uart0_peek(void)
{
	uint16_t tmptail;
	uint8_t data;

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if (UART_RxHead == UART_RxTail) {
			return UART_NO_DATA;   /* no data available */
		}
	}
	
	tmptail = (UART_RxTail + 1) & UART_RX0_BUFFER_MASK;

	/* get data from receive buffer */
	data = UART_RxBuf[tmptail];

	return (UART_LastRxError << 8) + data;

} /* uart0_peek */

/*************************************************************************
Function: uart0_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none
**************************************************************************/
void uart0_putc(uint8_t data)
{

#ifdef USART0_LARGE_BUFFER
	uint16_t tmphead;
	uint16_t txtail_tmp;

	tmphead = (UART_TxHead + 1) & UART_TX0_BUFFER_MASK;

	do {
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			txtail_tmp = UART_TxTail;
		}
	} while (tmphead == txtail_tmp); /* wait for free space in buffer */
#else
	uint16_t tmphead;
	
	tmphead = (UART_TxHead + 1) & UART_TX0_BUFFER_MASK;
	
	while (tmphead == UART_TxTail); /* wait for free space in buffer */
#endif

	UART_TxBuf[tmphead] = data;
	UART_TxHead = tmphead;

	/* enable UDRE interrupt */
	UART0_CONTROL |= _BV(UART0_UDRIE);

} /* uart0_putc */


/*************************************************************************
Function: uart0_puts()
Purpose:  transmit string to UART
Input:    string to be transmitted
Returns:  none
**************************************************************************/
void uart0_puts(const char *s)
{
	while (*s) {
		uart0_putc(*s++);
	}

} /* uart0_puts */


/*************************************************************************
Function: uart0_puts_p()
Purpose:  transmit string from program memory to UART
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/
void uart0_puts_p(const char *progmem_s)
{
	register char c;

	while ((c = pgm_read_byte(progmem_s++))) {
		uart0_putc(c);
	}

} /* uart0_puts_p */



/*************************************************************************
Function: uart0_available()
Purpose:  Determine the number of bytes waiting in the receive buffer
Input:    None
Returns:  Integer number of bytes in the receive buffer
**************************************************************************/
uint16_t uart0_available(void)
{
	uint16_t ret;
	
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		ret = (UART_RX0_BUFFER_SIZE + UART_RxHead - UART_RxTail) & UART_RX0_BUFFER_MASK;
	}
	return ret;
} /* uart0_available */

/*************************************************************************
Function: uart0_flush()
Purpose:  Flush bytes waiting the receive buffer. Actually ignores them.
Input:    None
Returns:  None
**************************************************************************/
void uart0_flush(void)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		UART_RxHead = UART_RxTail;
	}
} /* uart0_flush */

/*************************************************************************
Function: uart0_errchk()
Purpose:  checks the error bytes and transmits an error message via UART in 
		  case of an error
Input:    None
Returns:  boolean false if no error was found; true if an error occured
**************************************************************************/
uint16_t uart0_errchk(uint16_t rec_val){
	
	if (rec_val & UART_FRAME_ERROR ){
		uart0_puts("UART_FRAME_ERROR occurred!");
		return UART_FRAME_ERROR;
	}
	else if (rec_val & UART_OVERRUN_ERROR){
		uart0_puts("UART_OVERRUN_ERROR occurred!");
		return UART_OVERRUN_ERROR;
	}
	else if (rec_val & UART_BUFFER_OVERFLOW){
		uart0_puts("UART_BUFFER_OVERFLOW occurred!");
		return UART_BUFFER_OVERFLOW;
	}
	else if (rec_val & UART_NO_DATA){
		uart0_puts("UART_NO_DATA occurred!");
		return UART_NO_DATA;
	}
	return 0;
}

/*************************************************************************
Function: uart0_getln()
Purpose:  reads a line from UART buffer (delimiter); '\b' and DEL=127 are 
		  ignored and the most recent chr is deleted
Input:    pointer to a line buffer
Returns:  0x00 no bytes available
		  0x01 one line was read successfully
		  0x02 UART transmit Error occurred
**************************************************************************/
uint16_t uart0_getln(char* uart0_line_buf)
{
	if (uart0_available() > 0){
		//Bytes received
		static uint8_t uart0_line_buf_len = 0;
		
		uint16_t rec_val;		//received value
		char rec_c;				//received character
		
		rec_val = uart0_getc();
		rec_c = (char)rec_val;	//lower 8 bit
		
		//Check for receive errors
		if ( uart0_errchk(rec_val) ){
			return uart0_errchk(rec_val);
		}

		// Process character
		// mit peak \n\r abfangen!
		
		if ( rec_c == LINE_DELIMITER ){
			//EOL reached
			
			if (uart0_line_buf_len != 0){
				//reset buffer index
				uart0_line_buf_len = 0;
			}
			else{
				//first character was a delimiter -> set terminator to first buffer index
				//(empty string)
				uart0_line_buf[uart0_line_buf_len] = 0;
			}
			return 0x01;
		}
		else {
			//EOL not reached 
			
			//Ignore backspace and "DEL" (=127)
			if ( rec_c == '\b' || rec_c == 127 ){
				//delete the most recent character
				uart0_line_buf_len--;
			}
			else{
				//-> store to buffer
				if(uart0_line_buf_len < LINE_BUF_SIZE){
					uart0_line_buf[uart0_line_buf_len++] = rec_c;
					uart0_line_buf[uart0_line_buf_len] = 0; // append the null terminator
				}
				else{
					//buffer full -> print error message
					uart0_puts("Line length exceeds buffer!");
				}
			}
		}
	}
	return 0x00;
}
#endif /* defined(USART0_ENABLED) */























#if defined(USART1_ENABLED)

/*
 * these functions are only for ATmegas with two USART
 */
#if defined(ATMEGA_USART1)

ISR(UART1_RECEIVE_INTERRUPT)
/*************************************************************************
Function: UART1 Receive Complete interrupt
Purpose:  called when the UART1 has received a character
**************************************************************************/
{
	uint16_t tmphead;
	uint8_t data;
	uint8_t usr;
	uint8_t lastRxError;

	/* read UART status register and UART data register */
	usr  = UART1_STATUS;
	data = UART1_DATA;

	/* */
	lastRxError = (usr & (_BV(FE1)|_BV(DOR1)));

	/* calculate buffer index */
	tmphead = (UART1_RxHead + 1) & UART_RX1_BUFFER_MASK;

	if (tmphead == UART1_RxTail) {
		/* error: receive buffer overflow */
		lastRxError = UART_BUFFER_OVERFLOW >> 8;
	} else {
		/* store new index */
		UART1_RxHead = tmphead;
		/* store received data in buffer */
		UART1_RxBuf[tmphead] = data;
	}
	UART1_LastRxError = lastRxError;
}


ISR(UART1_TRANSMIT_INTERRUPT)
/*************************************************************************
Function: UART1 Data Register Empty interrupt
Purpose:  called when the UART1 is ready to transmit the next byte
**************************************************************************/
{
	uint16_t tmptail;

	if (UART1_TxHead != UART1_TxTail) {
		/* calculate and store new buffer index */
		tmptail = (UART1_TxTail + 1) & UART_TX1_BUFFER_MASK;
		UART1_TxTail = tmptail;
		/* get one byte from buffer and write it to UART */
		UART1_DATA = UART1_TxBuf[tmptail];  /* start transmission */
	} else {
		/* tx buffer empty, disable UDRE interrupt */
		UART1_CONTROL &= ~_BV(UART1_UDRIE);
	}
}


/*************************************************************************
Function: uart1_init()
Purpose:  initialize UART1 and set baudrate
Input:    baudrate using macro UART_BAUD_SELECT()
Returns:  none
**************************************************************************/
void uart1_init(uint16_t baudrate)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {	
		UART1_TxHead = 0;
		UART1_TxTail = 0;
		UART1_RxHead = 0;
		UART1_RxTail = 0;
	}

	/* Set baud rate */
	if (baudrate & 0x8000) {
		UART1_STATUS = (1<<U2X1);  //Enable 2x speed
		baudrate &= ~0x8000;
	}
	UBRR1H = (uint8_t) (baudrate>>8);
	UBRR1L = (uint8_t) baudrate;

	/* Enable USART receiver and transmitter and receive complete interrupt */
	UART1_CONTROL = _BV(RXCIE1)|(1<<RXEN1)|(1<<TXEN1);

	/* Set frame format: asynchronous, 8data, no parity, 1stop bit */
	#ifdef URSEL1
		UCSR1C = (1<<URSEL1)|(3<<UCSZ10);
	#else
		UCSR1C = (3<<UCSZ10);
	#endif
} /* uart_init */


/*************************************************************************
Function: uart1_getc()
Purpose:  return byte from ringbuffer
Returns:  lower byte:  received byte from ringbuffer
          higher byte: last receive error
**************************************************************************/
uint16_t uart1_getc(void)
{
	uint16_t tmptail;
	uint8_t data;

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if (UART1_RxHead == UART1_RxTail) {
			return UART_NO_DATA;   /* no data available */
		}

		/* calculate / store buffer index */
		tmptail = (UART1_RxTail + 1) & UART_RX1_BUFFER_MASK;
		UART1_RxTail = tmptail;
	}

	/* get data from receive buffer */
	data = UART1_RxBuf[tmptail];

	return (UART1_LastRxError << 8) + data;

} /* uart1_getc */

/*************************************************************************
Function: uart1_peek()
Purpose:  Returns the next byte (character) of incoming UART data without
          removing it from the ring buffer. That is, successive calls to
		  uartN_peek() will return the same character, as will the next
		  call to uartN_getc()
Returns:  lower byte:  next byte in ring buffer
          higher byte: last receive error
**************************************************************************/
uint16_t uart1_peek(void)
{
	uint16_t tmptail;
	uint8_t data;

	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		if (UART1_RxHead == UART1_RxTail) {
			return UART_NO_DATA;   /* no data available */
		}
	}
	
	tmptail = (UART1_RxTail + 1) & UART_RX1_BUFFER_MASK;

	/* get data from receive buffer */
	data = UART1_RxBuf[tmptail];

	return (UART1_LastRxError << 8) + data;

} /* uart1_peek */

/*************************************************************************
Function: uart1_putc()
Purpose:  write byte to ringbuffer for transmitting via UART
Input:    byte to be transmitted
Returns:  none
**************************************************************************/
void uart1_putc(uint8_t data)
{
	
#ifdef USART1_LARGE_BUFFER
	uint16_t tmphead;
	uint16_t txtail_tmp;

	tmphead = (UART1_TxHead + 1) & UART_TX1_BUFFER_MASK;

	do {
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			txtail_tmp = UART1_TxTail;
		}
	} while (tmphead == txtail_tmp); /* wait for free space in buffer */
#else
	uint16_t tmphead;
	
	tmphead = (UART1_TxHead + 1) & UART_TX1_BUFFER_MASK;
	
	while (tmphead == UART1_TxTail); /* wait for free space in buffer */
#endif	

	UART1_TxBuf[tmphead] = data;
	UART1_TxHead = tmphead;

	/* enable UDRE interrupt */
	UART1_CONTROL |= _BV(UART1_UDRIE);

} /* uart1_putc */


/*************************************************************************
Function: uart1_puts()
Purpose:  transmit string to UART1
Input:    string to be transmitted
Returns:  none
**************************************************************************/
void uart1_puts(const char *s)
{
	while (*s) {
		uart1_putc(*s++);
	}

} /* uart1_puts */


/*************************************************************************
Function: uart1_puts_p()
Purpose:  transmit string from program memory to UART1
Input:    program memory string to be transmitted
Returns:  none
**************************************************************************/
void uart1_puts_p(const char *progmem_s)
{
	register char c;

	while ((c = pgm_read_byte(progmem_s++))) {
		uart1_putc(c);
	}

} /* uart1_puts_p */



/*************************************************************************
Function: uart1_available()
Purpose:  Determine the number of bytes waiting in the receive buffer
Input:    None
Returns:  Integer number of bytes in the receive buffer
**************************************************************************/
uint16_t uart1_available(void)
{
	uint16_t ret;
	
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		ret = (UART_RX1_BUFFER_SIZE + UART1_RxHead - UART1_RxTail) & UART_RX1_BUFFER_MASK;
	}
	return ret;
} /* uart1_available */



/*************************************************************************
Function: uart1_flush()
Purpose:  Flush bytes waiting the receive buffer. Actually ignores them.
Input:    None
Returns:  None
**************************************************************************/
void uart1_flush(void)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		UART1_RxHead = UART1_RxTail;
	}
} /* uart1_flush */

#endif

#endif /* defined(USART1_ENABLED) */