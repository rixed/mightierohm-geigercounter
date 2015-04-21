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

#ifndef BUBBLE_LED_VIA_SHIFT_REGISTER
# error "YO!"
  // bit 0 to cathode 1, etc, and bit 3 to cathode 4
# ifndef BB_PORT_SEGMENTS
#   define BB_PORT_SEGMENTS B
# endif
#else // BUBBLE_LED_VIA_SHIFT_REGISTER
  // In this case you want the shirt register connected to a given port:
# ifndef BB_PORT_SHIFT_REG
#   define BB_PORT_SHIFT_REG B
# endif
#endif

// Your display must be connected with bit 0 to a, bit 1 to b, etc, and bit 7 to dp.
#ifndef BB_PORT_DIGITS
# define BB_PORT_DIGITS D
#endif

// You must call event_init first.
void bubble_init(void);

struct bubble {
  struct event e;
  uint8_t dx; // which digit to display next
  uint8_t digits[4];  // 0 at startup (likely)
};

void bubble_alternate_digit(struct event *);

static inline void bubble_ctor(struct bubble *b)
{
  event_ctor(&b->e, bubble_alternate_digit);
  b->dx = 0;
  event_register(&b->e, 0);
}

void bubble_set_float(struct bubble *, uint16_t value, uint8_t dec_bits);

// digit must be between 0 and 3, value between 0 and 9.
void bubble_set(uint8_t digit, uint8_t value, bool dp);

#endif
