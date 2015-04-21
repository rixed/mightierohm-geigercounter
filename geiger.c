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
// overwrite parameter from CLI because on the board we are using this freq.
#undef  F_CPU
#define F_CPU    8000000

// Includes
#include <stdlib.h>
#include <avr/io.h>     // this contains the AVR IO port definitions
#include <avr/interrupt.h>  // interrupt service routines
#include <avr/pgmspace.h> // tools used to store variables in program memory
#include <avr/sleep.h>    // sleep mode utilities
#include <util/delay.h>   // some convenient delay functions
#include "miscmacs.h"
#include "event.h"

// Defines
#define THRESHOLD   1000  // CPM threshold for fast avg mode
#define LONG_PERIOD   60    // # of samples to keep in memory in slow avg mode
#define SHORT_PERIOD  5   // # or samples for fast avg mode
#define SCALE_FACTOR  57    //  CPM to uSv/hr conversion factor (x10,000 to avoid float)

// Global variables
volatile uint16_t count;    // number of GM events that has occurred
volatile uint16_t slowcpm;    // GM counts per minute in slow mode
volatile uint16_t fastcpm;    // GM counts per minute in fast mode
volatile uint16_t cps;      // GM counts per second, updated once a second
volatile uint8_t overflow;    // overflow flag

volatile uint8_t buffer[LONG_PERIOD]; // the sample buffer
volatile uint8_t idx;         // sample buffer index

volatile uint8_t eventflag; // flag for ISR to tell main loop if a GM event has occurred
volatile uint8_t tick;    // flag that tells main() when 1 second has passed

// Interrupt service routines

//  Pin change interrupt for pin INT0
//  This interrupt is called on the falling edge of a GM pulse.
ISR(INT0_vect)
{
  if (count < UINT16_MAX) // check for overflow, if we do overflow just cap the counts at max possible
    count++; // increase event counter

  eventflag = 1;  // tell main program loop that a GM pulse has occurred
}

// Run this every seconds
static void periodic_event(struct event *e)
{
  // Reschedule
  event_register(e, US_TO_TIMER1_TICKS(1000000ULL));

  uint8_t i;  // index for fast mode
  tick = 1; // update flag

  PORTB ^= _BV(PB4);  // toggle the LED (for debugging purposes)
  cps = count;
  slowcpm -= buffer[idx];   // subtract oldest sample in sample buffer

  if (count > UINT8_MAX) {  // watch out for overflowing the sample buffer
    count = UINT8_MAX;
    overflow = 1;
  }

  slowcpm += count;     // add current sample
  buffer[idx] = count;  // save current sample to buffer (replacing old value)

  // Compute CPM based on the last SHORT_PERIOD samples
  fastcpm = 0;
  for(i=0; i<SHORT_PERIOD;i++) {
    int8_t x = idx - i;
    if (x < 0)
      x = LONG_PERIOD + x;
    fastcpm += buffer[x]; // sum up the last 5 CPS values
  }
  fastcpm = fastcpm * (LONG_PERIOD/SHORT_PERIOD); // convert to CPM

  // Move to the next entry in the sample buffer
  idx++;
  if (idx >= LONG_PERIOD)
    idx = 0;
  count = 0;  // reset counter
}

// Functions

// Send a character to the UART
static void uart_putchar(char c)
{
  if (c == '\n') uart_putchar('\r');  // Windows-style CRLF

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

// Send a string in PROGMEM to the UART
static void uart_putstring_P(char const *buffer)
{
  // start sending characters over the serial port until we reach the end of the string
  while (pgm_read_byte(buffer) != '\0') // are we done yet?
    uart_putchar(pgm_read_byte(buffer++));  // read byte from PROGMEM and send it
}

// flash LED and beep the piezo
static void checkevent(void)
{
  if (eventflag) {    // a GM event has occurred, do something about it!
    eventflag = 0;    // reset flag as soon as possible, in case another ISR is called while we're busy

    PORTB |= _BV(PB4);  // turn on the LED

    // Beep!
    TCCR0A |= _BV(COM0A0);  // enable OCR0A output on pin PB2
    TCCR0B |= _BV(CS01);  // set prescaler to clk/8 (1Mhz) or 1us/count
    OCR0A = 160;  // 160 = toggle OCR0A every 160ms, period = 320us, freq= 3.125kHz

    // 10ms delay gives a nice short flash and 'click' on the piezo
    // FIXME: do not delay here!
    _delay_ms(10);

    PORTB &= ~(_BV(PB4)); // turn off the LED

    TCCR0B = 0;       // disable Timer0 since we're no longer using it
    TCCR0A &= ~(_BV(COM0A0)); // disconnect OCR0A from Timer0, this avoids occasional HVPS whine after beep
  }
}
// log data over the serial port
static void sendreport(void)
{
  static enum mode_t {
    MODE_SLOW = 0,
    MODE_FAST = 1,
    MODE_INST = 2,
  } mode;

  uint32_t cpm; // This is the CPM value we will report
  if(tick) {  // 1 second has passed, time to report data via UART
    tick = 0; // reset flag for the next interval

    if (overflow) {
      cpm = cps*60UL;
      mode = MODE_INST;
      overflow = 0;
    }
    else if (fastcpm > THRESHOLD) { // if cpm is too high, use the short term average instead
      mode = MODE_FAST;
      cpm = fastcpm;  // report cpm based on last 5 samples
    } else {
      mode = MODE_SLOW;
      cpm = slowcpm;  // report cpm based on last 60 samples
    }

    // Send CPM value to the serial port
    uart_putuint(cps);
    uart_putstring_P(PSTR(","));

    uart_putuint(cpm);
    uart_putstring_P(PSTR(","));

    // calculate uSv/hr based on scaling factor, and multiply result by 100
    // so we can easily separate the integer and fractional components (2 decimal places)
    uint32_t usv_scaled = (uint32_t)(cpm*SCALE_FACTOR); // scale and truncate the integer part

    // this reports the integer part
    uart_putuint((uint16_t)(usv_scaled/10000));

    uart_putchar('.');

    // this reports the fractional part (2 decimal places)
    uint8_t fraction = (usv_scaled/100)%100;
    if (fraction < 10)
      uart_putchar('0');  // zero padding for <0.10
    uart_putuint(fraction);

    // Tell us what averaging method is being used
    switch (mode) {
      case MODE_INST:
        uart_putstring_P(PSTR(",I"));
        break;
      case MODE_FAST:
        uart_putstring_P(PSTR(",F"));
        break;
      case MODE_SLOW:
        uart_putstring_P(PSTR(",S"));
        break;
    }

    // We're done reporting data, output a newline.
    uart_putchar('\n');
  }
}

// Start of main program
int main(void)
{
  // Configure the UART
  // Set baud rate generator based on F_CPU
  UBRRH = (unsigned char)(F_CPU/(16UL*BAUD)-1)>>8;
  UBRRL = (unsigned char)(F_CPU/(16UL*BAUD)-1);

  // Enable USART transmitter and receiver
  UCSRB = (1<<RXEN) | (1<<TXEN);

  uart_putstring_P(PSTR("mightyohm.com Geiger Counter\n"));

  // Set up AVR IO ports
  DDRB = _BV(PB4) | _BV(PB2);  // set pins connected to LED and piezo as outputs

  // Set up external interrupts
  // INT0 is triggered by a GM impulse
  MCUCR |= _BV(ISC01); // Config interrupts on falling edge of INT0
  GIMSK |= _BV(INT0);  // Enable external interrupts on pin INT0

  // Configure the Timers
  // Set up Timer0 for tone generation
  // Toggle OC0A (pin PB2) on compare match and set timer to CTC mode
  TCCR0A = (0<<COM0A1) | (1<<COM0A0) | (0<<WGM02) |  (1<<WGM01) | (0<<WGM00);
  TCCR0B = 0; // stop Timer0 (no sound)

  event_init();
  struct event e;
  event_ctor(&e, periodic_event);
  periodic_event(&e);

  while(1) {  // loop forever

    // Configure AVR for sleep, this saves a couple mA when idle
    set_sleep_mode(SLEEP_MODE_IDLE);  // CPU will go to sleep but peripherals keep running
    sleep_enable();   // enable sleep
    sleep_cpu();    // put the core to sleep

    // Zzzzzzz... CPU is sleeping!
    // Execution will resume here when the CPU wakes up.

    sleep_disable();  // disable sleep so we don't accidentally go to sleep

    checkevent(); // check if we should signal an event (led + beep)

    sendreport(); // send a log report over serial

    checkevent(); // check again before going to sleep

  }
  return 0; // never reached
}

