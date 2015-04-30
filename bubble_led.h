/* Small "driver" for the QDSP 6064 Micro Numeric 7 segments Bubble display.
 * Datasheet: http://cdn.sparkfun.com/datasheets/Components/LED/BB_QDSP_DS.pdf
 *
 * We need 8 pins for the leds and 4 for the digit selection.
 */
#include <stdint.h>
#include <stdbool.h>
#include "event.h"
#ifndef BUBBLE_LED_H_150417
#define BUBBLE_LED_H_150417

// You must define those two functions:
// bit 0 to cathode 1, etc, and bit 3 to cathode 4
void set_digits(uint8_t);
// Your display must be connected with bit 0 to a, bit 1 to b, etc, and bit 7 to dp.
void set_segments(uint8_t);

struct bubble {
  struct event e;
  uint8_t dx; // which digit to display next
  uint8_t digits[4];  // 0 at startup (likely)
  uint8_t dp;
};

void bubble_alternate_digit(struct event *);

static inline void bubble_ctor(struct bubble *b)
{
  event_ctor(&b->e);
  b->dx = 0;
  bubble_alternate_digit(&b->e);
}

// dp: after which digit to set the decimal point (if between 0 and 3).
void bubble_set_float(struct bubble *, uint16_t value, uint8_t dp);

// digit must be between 0 and 3, value between 0 and 9.
void bubble_set(uint8_t digit, uint8_t value, bool dp);

#endif
