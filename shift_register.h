/* Small "driver" for the 74HC595 8bits shift register.
 * All three bits (DataSerial, SHiftClockinPut, StoReClockinPut)
 * must be in the same port, 3 lower bits, in that order.
 */
#include <stdint.h>
#include "miscmacs.h"
#include "event.h"
#ifndef SHIFT_REG_H_150420
#define SHIFT_REG_H_150420

inline static void shift_reg_put(volatile uint8_t *port, uint8_t value)
{
# define DS   0
# define SHCP 1
# define STCP 2
  BIT_CLEAR(*port, STCP);
  for (uint8_t bit = 0x80U; bit; bit >>= 1U) {
    if (value & bit) BIT_SET(*port, DS);
    else BIT_CLEAR(*port, DS);
    BIT_SET(*port, SHCP);
    BIT_CLEAR(*port, SHCP);
  }

  BIT_SET(*port, STCP);
}

#endif
