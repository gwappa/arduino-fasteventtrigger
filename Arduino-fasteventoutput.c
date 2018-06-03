/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.

  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the Arduino-usbtrigger project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "Arduino-fasteventoutput.h"

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = 0,

				.DataINEndpointNumber           = CDC_TX_EPNUM,
				.DataINEndpointSize             = CDC_TXRX_EPSIZE,
				.DataINEndpointDoubleBank       = false,

				.DataOUTEndpointNumber          = CDC_RX_EPNUM,
				.DataOUTEndpointSize            = CDC_TXRX_EPSIZE,
				.DataOUTEndpointDoubleBank      = false,

				.NotificationEndpointNumber     = CDC_NOTIFICATION_EPNUM,
				.NotificationEndpointSize       = CDC_NOTIFICATION_EPSIZE,
				.NotificationEndpointDoubleBank = false,
			},
	};

//#define SYNC_PULSES

#define PORT_OUT   PORTB
#define IO_REGISTER DDRB
// SYNC: PB1 (pin 15)
#define MASK_SYNC  (0x01<<1)
// POWER: PB2 (pin 16)
#define MASK_POWER (0x01<<2)
// EVENT: PB3 (pin 17)
#define MASK_EVENT (0x01<<3)

#define	SYNC_ON		((uint8_t)'1')
#define SYNC_OFF 	((uint8_t)'2')
#define EVENT_ON	((uint8_t)'A')
#define EVENT_OFF 	((uint8_t)'D')
#define FLUSH_BUF 	((uint8_t)'F')
#define CLEAR_BUF   ((uint8_t)'O')
	  uint8_t	command 	= (uint8_t)'\0';

	  char 		state 		= '\0';
	  char  	buf 		= '\0';
const char 		FLAG_SYNC	= (char)0x01;
const char 		FLAG_EVENT 	= (char)0x02;
	  uint8_t	offset 		= 0;

#ifdef SYNC_PULSES
      bool      pulse       = false;

ISR(TIMER1_OVF_vect) {
    cli();
    if (pulse) {
        PORT_OUT &= ~MASK_SYNC;
        pulse = false;
    }
    sei();
}

inline void initTimer(void) {
    TCCR1A  = 0; // normal mode
    TCCR1B  = (1 << CS12); // prescaler = 256
    TIMSK1  = (1 << TOIE1);
}

inline void startSyncTimer(void) {
    cli();
    PORT_OUT |= MASK_SYNC;
    pulse     = true;

    TCNT1   = 0xFF01;
    sei();
}
#else
inline void initTimer(void) {
    // do nothing
}
#endif

inline void syncOn(void) {
    if ( (state & FLAG_SYNC) == 0 ) {
#ifdef SYNC_PULSES
        startSyncTimer();
#else
        PORT_OUT |= MASK_SYNC;
#endif
        state    |= FLAG_SYNC;
    }
}

inline void syncOff(void) {
    if (state & FLAG_SYNC) {
#ifdef SYNC_PULSES
        startSyncTimer();
#else
        PORT_OUT &= ~MASK_SYNC;
#endif
        state    &= ~FLAG_SYNC;
    }
}

inline void eventOn(void) {
    if ( (state & FLAG_EVENT) == 0 ) {
        PORT_OUT |= MASK_EVENT;
        state    |= FLAG_EVENT;
    }
}

inline void eventOff(void) {
    if (state & FLAG_EVENT) {
        PORT_OUT &= ~MASK_EVENT;
        state    &= ~FLAG_EVENT;
    }
}


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

    initTimer();

	sei();

	offset = 0;

	IO_REGISTER |= MASK_POWER;
	IO_REGISTER |= MASK_SYNC;
	IO_REGISTER |= MASK_EVENT;

	PORT_OUT |= MASK_POWER;

	for (;;)
	{
		int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

		/* Read bytes from the USB OUT endpoint into the USART transmit buffer */
		if (!(ReceivedByte < 0)){
			command = (uint8_t)ReceivedByte;
			switch(command){
			case SYNC_ON:
                syncOn();
				buf       = (buf << 2) | state;
				offset++;
				break;
			case SYNC_OFF:
                syncOff();
				buf       = (buf << 2) | state;
				offset++;
				break;
			case EVENT_ON:
                eventOn();
				buf       = (buf << 2) | state;
				offset++;
				break;
			case EVENT_OFF:
                eventOff();
				buf       = (buf << 2) | state;
				offset++;
				break;
            // in case of multiplexed commands
            case (SYNC_ON | EVENT_ON):
                eventOn();
                syncOn();
                buf       = (buf << 2) | state;
                offset++;
                break;
            case (SYNC_ON | EVENT_OFF):
                eventOff();
                syncOn();
                buf       = (buf << 2) | state;
                offset++;
                break;
            case (SYNC_OFF | EVENT_ON):
                eventOn();
                syncOff();
                buf       = (buf << 2) | state;
                offset++;
                break;
            case (SYNC_OFF | EVENT_OFF):
                eventOff();
                syncOff();
                buf       = (buf << 2) | state;
                offset++;
                break;
            // in case of buffer reset commands
			case FLUSH_BUF:
				for(;offset<4;offset++){
					buf = (buf << 2) | state;
				}
				break;
			case CLEAR_BUF:
				PORT_OUT &= ~(MASK_SYNC | MASK_EVENT);
				state     = 0x00;
				offset    = 0;
				break;
			// do nothing by default
			}
			if( offset == 4 ){
				CDC_Device_SendByte(&VirtualSerial_CDC_Interface, buf);
				offset = 0;
			}
		}

		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Hardware Initialization */
	USB_Init();

	/* Start the flush timer so that overflows occur rapidly to push received bytes to the USB interface */
	TCCR0B = (1 << CS02);

	/* Pull target /RESET line high */
	AVR_RESET_LINE_PORT |= AVR_RESET_LINE_MASK;
	AVR_RESET_LINE_DDR  |= AVR_RESET_LINE_MASK;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Unhandled Control Request event. */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	uint8_t ConfigMask = 0;

	switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
	{
		case CDC_PARITY_Odd:
			ConfigMask = ((1 << UPM11) | (1 << UPM10));
			break;
		case CDC_PARITY_Even:
			ConfigMask = (1 << UPM11);
			break;
	}

	if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
	{
		case 6:
			ConfigMask |= (1 << UCSZ10);
			break;
		case 7:
			ConfigMask |= (1 << UCSZ11);
			break;
		case 8:
			ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
			break;
	}

	/* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;

	/* Special case 57600 baud for compatibility with the ATmega328 bootloader. */
	UBRR1  = (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 57600)
			 ? SERIAL_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS)
			 : SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

	UCSR1C = ConfigMask;
	UCSR1A = (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 57600) ? 0 : (1 << U2X1);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));
}

/** Event handler for the CDC Class driver Host-to-Device Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	bool CurrentDTRState = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR);

	if (CurrentDTRState)
	  AVR_RESET_LINE_PORT &= ~AVR_RESET_LINE_MASK;
	else
	  AVR_RESET_LINE_PORT |= AVR_RESET_LINE_MASK;
}
