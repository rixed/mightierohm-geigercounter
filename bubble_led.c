#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include "miscmacs.h"
#ifdef BUBBLE_LED_VIA_SHIFT_REGISTER
# include "shift_register.h"
#endif
#include "bubble_led.h"

static uint8_t segments_of_value(uint8_t value)
{
  /* In normal C, you want a switch and let the compiler eventually
   * optimize it into an array. Here the array would be in RAM,
   * eating up both flash size and requiring the copy of the array
   * from flash to RAM, which also consume flash. So we explicitly
   * perform the array optimization, but keep it in flash so that,
   * should we have no other constant data, we avoid _do_copy_data
   */
  static uint8_t const segs[10] PROGMEM = {
#   define __ 0U
#   define sA 1U
#   define sB 2U
#   define sC 4U
#   define sD 8U
#   define sE 16U
#   define sF 32U
#   define sG 64U
    sA+sB+sC+sD+sE+sF+__,
    __+sB+sC+__+__+__+__,
    sA+sB+__+sD+sE+__+sG,
    sA+sB+sC+sD+__+__+sG,
    __+sB+sC+__+__+sF+sG,
    sA+__+sC+sD+__+sF+sG,
    sA+__+sC+sD+sE+sF+sG,
    sA+sB+sC+__+__+__+__,
    sA+sB+sC+sD+sE+sF+sG,
    sA+sB+sC+sD+__+sF+sG,
  };
  if (value < SIZEOF_ARRAY(segs)) return pgm_read_byte(segs+value);
  return sA+__+__+sD+__+__+sG;
}

#define X_PORT_OF(X) PORT ## X
#define PORT_OF(X) X_PORT_OF(X)

#define X_DDR_OF(X) DDR ## X
#define DDR_OF(X) X_DDR_OF(X)

void bubble_set_float(struct bubble *b, uint16_t value, uint8_t dec_bits)
{
  (void)dec_bits;
  // v1: cassume dec_bits = 0
  // Avoid doing a useless division
  uint8_t d;
  for (d = 0; d < SIZEOF_ARRAY(b->digits); d++) {
    div_t const r = udiv(value, 10U);
    b->digits[d] = r.rem;
    value = r.quot;
  }
}

static void bubble_off(void)
{
# ifndef BUBBLE_LED_VIA_SHIFT_REGISTER
  PORT_OF(BB_PORT_SEGMENTS) = 0;
# else
  // TODO: faster using the MR pin of the shift_reg.
  shift_reg_put(&PORT_OF(BB_PORT_SHIFT_REG), 0);
# endif
}

void bubble_set(uint8_t digit, uint8_t value, bool dp)
{
  // Do not allow another digit to leak light into this one,
  // which is especially visible if we use the shift register.
  bubble_off();
  PORT_OF(BB_PORT_DIGITS) = (PORT_OF(BB_PORT_DIGITS) & 0xf0U) | (0x0fU ^ BIT(3-digit));
  uint8_t const s = segments_of_value(value) + (dp ? 128U:0U);
# ifndef BUBBLE_LED_VIA_SHIFT_REGISTER
  PORT_OF(BB_PORT_SEGMENTS) = s;
# else  // segments are accessed via a shift register, accessible via port BB_PORT_SHIFT_REG
  shift_reg_put(&PORT_OF(BB_PORT_SHIFT_REG), s);
# endif
}

#define DIGIT_ALTERN_US 1000U
void bubble_alternate_digit(struct event *e)
{
  struct bubble *b = DOWNCAST(e, e, bubble);

  bubble_set(b->dx, b->digits[b->dx], false);

  if (++b->dx >= SIZEOF_ARRAY(b->digits)) b->dx = 0;

  event_register(&b->e, US_TO_TIMER1_TICKS(DIGIT_ALTERN_US));
}

extern inline void bubble_ctor(struct bubble *b);

void bubble_init(void)
{
  DDR_OF(BB_PORT_DIGITS) |= 0x0fU;
# ifndef BUBBLE_LED_VIA_SHIFT_REGISTER
  DDR_OF(BB_PORT_SEGMENTS) = 0xffU;
# else
  DDR_OF(BB_PORT_SHIFT_REG) |= 0x07U;
# endif
}
