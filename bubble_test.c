/* Test the Bubble led display.
 */
// Try with the shift register:
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include "miscmacs.h"
#include "bubble_led.h"
#include "event.h"

static struct bubble b;

static void inc_counter(struct event *e)
{
  static uint16_t count = 0;
  bubble_set_float(&b, count, 0);
  //if (++ count > 9999) count = 0;
  //count = div(count+1, 10000).quot;
  count ++;
  count %= 10000;
  event_register(e, US_TO_TIMER1_TICKS(100000U));
}

int main(void)
{
  event_init();
  bubble_init();

  bubble_ctor(&b);

  static struct event counter_event;
  event_ctor(&counter_event, inc_counter);
  inc_counter(&counter_event);
  forever {
  }
}
