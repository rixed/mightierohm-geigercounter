/* This is a total conversion of the Geiger Counter from mightyohm.com.
 * Modifications:
 * - Addition of a 4 digits led display to display current radiation level;
 * - Addition of a SIPO shift register to help driving the display;
 * - Removal of the LED to save one pin;
 * - Removal of the pulse feature to save one pin;
 * - Consider replacement of the mute button by a switch to save one pin of
 *   the uC;
 * - Major rewrite of the firmware (this file);
 * - Making all code fit within 2Kb requires trimming down texts.
 *
 * We had 5 free pins in the original design, we now have 8, which is enough
 * for driving the display with the help of a SIPO shift register.
 * We actually need:
 * - 4 pins to select what digit to lit;
 * - 3 pins to talk to the SIPO.
 *
 * What follows is the original description: */
/*
  Title: Geiger Counter with Serial Data Reporting
  Description: This is the firmware for the mightyohm.com Geiger Counter.
    There is more information at http://mightyohm.com/geiger

  Author:   Jeff Keyzer
  Company:  MightyOhm Engineering
  Website:  http://mightyohm.com/
  Contact:  jeff <at> mightyohm.com

  This firmware controls the ATtiny2313 AVR microcontroller on board the Geiger Counter kit.

  When an impulse from the GM tube is detected, the firmware flashes the LED and produces a short
  beep on the piezo speaker.  It also outputs an active-high pulse (default 100us) on the PULSE pin.

  A pushbutton on the PCB can be used to mute the beep.

  A running average of the detected counts per second (CPS), counts per minute (CPM), and equivalent dose
  (uSv/hr) is output on the serial port once per second. The dose is based on information collected from
  the web, and may not be accurate.

  The serial port is configured for BAUD baud, 8-N-1 (default 9600).

  The data is reported in comma separated value (CSV) format:
  CPS, CPM, uSv/hr, S(low)|F(ast)|I(nst)

  There are three modes.  Normally, the sample period is LONG_PERIOD (default 60 seconds). This is SLOW averaging mode.
  If the last five measured counts exceed a preset threshold, the sample period switches to SHORT_PERIOD seconds (default 5 seconds).
  This is FAST mode, and is more responsive but less accurate. Finally, if CPS > 255, we report CPS*60 and switch to
  INST mode, since we can't store data in the (8-bit) sample buffer.  This behavior could be customized to suit a particular
  logging application.

  The largest CPS value that can be displayed is 65535, but the largest value that can be stored in the sample buffer is 255.

  ***** WARNING *****
  This Geiger Counter kit is for EDUCATIONAL PURPOSES ONLY.  Don't even think about using it to monitor radiation in
  life-threatening situations, or in any environment where you may expose yourself to dangerous levels of radiation.
  Don't rely on the collected data to be an accurate measure of radiation exposure! Be safe!


  Change log:
  8/4/11 1.00: Initial release for Chaos Camp 2011!


    Copyright 2011 Jeff Keyzer, MightyOhm Engineering

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// Includes
#include <stdlib.h>
#include <avr/io.h>     // this contains the AVR IO port definitions
#include <avr/interrupt.h>  // interrupt service routines
#include <avr/pgmspace.h> // tools used to store variables in program memory
#include <avr/sleep.h>    // sleep mode utilities
#include "miscmacs.h"
#include "event.h"
#include "shift_register.h"
#include "bubble_led.h"

// Defines
#define THRESHOLD   1000  // CPM threshold for fast avg mode
#define SCALE_FACTOR  57    //  CPM to uSv/hr conversion factor (x10,000 to avoid float)

// Global variables
static volatile uint8_t cps;     // number of GM events that has occurred this second

static struct bubble bubble;

/* Utility functions */

#ifdef WITH_COM
// Send a character to the UART
static void uart_putchar(char c)
{
  if (c == '\n') c = '\r';  // Windows-style CRLF

  loop_until_bit_is_set(UCSRA, UDRE); // wait until UART is ready to accept a new character
  UDR = c;              // send 1 character
}

static void uart_putuint(uint32_t x)
{
  if (x < 10) {
    uart_putchar(x + '0');
  } else {
    ldiv_t const r = uldiv(x, 10U);
    uart_putuint(r.quot);
    uart_putuint(r.rem);
  }
}
#endif

/* Events */

static struct event every_second_e;
static struct event bip_stop_e;

// Run this every seconds
static void every_second(struct event *e)
{
  static uint8_t buffer[30]; // the sample buffer. Any 255 means overflow.
  static uint8_t idx;         // sample buffer index

  //BIT_FLIP(PORTB, PB4);  // toggle the LED (for debugging purposes)

  uint8_t const c_cps = cps;  // non valoatile copy
  cps = 0;  // reset counter

  buffer[idx] = c_cps;   // save current sample to buffer (replacing old value)
  if (++idx >= SIZEOF_ARRAY(buffer)) idx = 0;

# ifdef WITH_COM
  // Log data over the serial port
  if (c_cps >= UINT8_MAX) uart_putchar('>');
  uart_putuint(c_cps);
  uart_putchar(',');
  uint8_t overflow = 0;
# endif

  uint16_t cpm = 0;
  for (uint8_t i = 0; i < SIZEOF_ARRAY(buffer); i++) {
#   ifdef WITH_COM
    if (buffer[i] == UINT8_MAX) overflow = 1;
#   endif
    cpm += buffer[i];
  }
  cpm <<= 1;  // since we have only 30secs
# ifdef WITH_COM
  if (overflow) uart_putchar('>');
  uart_putuint(cpm);
  uart_putchar(',');
# endif
  // Display CPM * 0.0057 aka something close to uSv/hr.
  // We keep one digit for the integral part and 3 for the decimal part
  // (otherwise you have bigger problems).
  // So we actually want 1000*uSv/hr. We thus mult by 5.7.
  uint16_t siv = (cpm * 1459UL) >> 8U;
  bubble_set_float(&bubble, siv, 3);

# ifdef WITH_COM
  uart_putuint(siv);
  uart_putchar('\n');
# endif

  // Reschedule
  event_register(e, every_second, US_TO_TIMER1_TICKS(1000000ULL));
}

static void bip_stop(struct event *e)
{
  (void)e;
  BIT_CLEAR(PORTB, PB4);
  TCCR0B = 0;       // disable Timer0 since we're no longer using it
  TCCR0A &= ~(_BV(COM0A0)); // disconnect OCR0A from Timer0, this avoids occasional HVPS whine after beep
}

// Bip start/stop events
static void bip_start(void)
{
  BIT_SET(PORTB, PB4);

  TCCR0A |= _BV(COM0A0);  // enable OCR0A output on pin PB2
  TCCR0B |= _BV(CS01);  // set prescaler to clk/8 (1Mhz) or 1us/count
  OCR0A = 160;  // 160 = toggle OCR0A every 160ms, period = 320us, freq= 3.125kHz

  // 10ms delay gives a nice short flash and 'click' on the piezo
  event_register(&bip_stop_e, bip_stop, US_TO_TIMER1_TICKS(10000ULL));  // 10ms
}

/* Interrupt */

//  Pin change interrupt for pin INT0
//  This interrupt is called on the falling edge of a GM pulse.
ISR(INT0_vect)
{
  uint8_t const c_cps = cps;  // non volatile copy
  if (c_cps < UINT8_MAX) // check for overflow, if we do overflow just cap the counts at max possible
    cps = c_cps + 1; // increase event counter

  bip_start();
}

/* Display driver callbacks */
void set_digits(uint8_t s)
{
  // We use PD6, PB0,1,3 for digits 0,1,2,3
  BIT_SET_TO(PORTD, 6, IS_BIT_SET(s, 0));
  BIT_SET_TO(PORTB, 0, IS_BIT_SET(s, 1));
  BIT_SET_TO(PORTB, 1, IS_BIT_SET(s, 2));
  BIT_SET_TO(PORTB, 3, IS_BIT_SET(s, 3));
}

void set_segments(uint8_t s)
{
  // We use PD3,4,5 to drive the SIPO through a shift register
  SHIFT_REG_PUT(PORTD, 3, PORTD, 4, PORTD, 5, s);
}

// Start of main program
int main(void)
{
  bubble_ctor(&bubble);

  // Configure the UART
  // Set baud rate generator based on F_CPU
  UBRRH = (unsigned char)(F_CPU/(16UL*BAUD)-1)>>8;
  UBRRL = (unsigned char)(F_CPU/(16UL*BAUD)-1);

  // Enable USART transmitter and receiver
  UCSRB = (1<<RXEN) | (1<<TXEN);

  // Set up AVR IO ports
  // PB4 is for the LED, PB2 for the piezzo, PB0,1,3 for digit selection:
  DDRB = _BV(PB4) | _BV(PB3) | _BV(PB2) | _BV(PB1) | _BV(PB0);
  // PD6 is also for digit selection, and PD3,4,5 to drive the SIPO:
  DDRD |= _BV(PD6) | _BV(PD5) | _BV(PD4) | _BV(PD3);

  // Set up external interrupts
  // INT0 is triggered by a GM impulse
  MCUCR |= _BV(ISC01); // Config interrupts on falling edge of INT0
  GIMSK |= _BV(INT0);  // Enable external interrupts on pin INT0

  // Set up Timer0 for tone generation
  TCCR0A = (0<<COM0A1) | (1<<COM0A0) | (0<<WGM02) |  (1<<WGM01) | (0<<WGM00);
  TCCR0B = 0; // stop Timer0 (no sound)

  event_init();
  event_ctor(&every_second_e);
  event_ctor(&bip_stop_e);
  every_second(&every_second_e);

  // Configure AVR for sleep, this saves a couple mA when idle
  set_sleep_mode(SLEEP_MODE_IDLE);  // CPU will go to sleep but peripherals keep running
  forever {  // loop forever
    sleep_enable();
    sei();
    sleep_cpu();    // put the core to sleep
  }
  return 0; // never reached
}
