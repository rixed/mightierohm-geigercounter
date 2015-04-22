/* Small "driver" for the 74HC595 8bits shift register.
 * All three bits (DataSerial, SHiftClockinPut, StoReClockinPut)
 * must be in the same port, 3 lower bits, in that order.
 */
#include <stdint.h>
#include "miscmacs.h"
#include "event.h"
#ifndef SHIFT_REG_H_150420
#define SHIFT_REG_H_150420

#define SHIFT_REG_PUT(DS_PORT, DS_BIT, SHCP_PORT, SHCP_BIT, STCP_PORT, STCP_BIT, value) do { \
  BIT_CLEAR(STCP_PORT, STCP_BIT); \
  for (uint8_t bit = 0x80U; bit; bit >>= 1U) { \
    if ((value) & bit) BIT_SET(DS_PORT, DS_BIT); \
    else BIT_CLEAR(DS_PORT, DS_BIT); \
    BIT_SET(SHCP_PORT, SHCP_BIT); \
    BIT_CLEAR(SHCP_PORT, SHCP_BIT); \
  } \
  BIT_SET(STCP_PORT, STCP_BIT); \
} while (0)

#endif
