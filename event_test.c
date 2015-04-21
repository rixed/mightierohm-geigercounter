/* Test at an event library using TIMER1 (16 bits) as a scheduler.
 */
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <avr/io.h>
#include "miscmacs.h"
#include "event.h"

#define LED1 PB0
#define LED2 PB1

void blink1(struct event *e)
{
  BIT_FLIP(PORTB, LED1);
  event_register(e, US_TO_TIMER1_TICKS(1000000U));
}

void blink2(struct event *e)
{
  BIT_FLIP(PORTB, LED2);
  event_register(e, US_TO_TIMER1_TICKS(1130000U));
}

int main(void)
{
  event_init();
  BIT_SET(DDRB, LED1);
  BIT_SET(DDRB, LED2);

  struct event led1_event;
  event_ctor(&led1_event, blink1);
  struct event led2_event;
  event_ctor(&led2_event, blink2);

  blink1(&led1_event);
  blink2(&led2_event);

  forever {
  }
}
