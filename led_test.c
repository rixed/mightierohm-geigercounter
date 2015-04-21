// Example 1 : Blinking a LED
#include <avr/io.h>
#include <util/delay.h>

#define LED PB0
#define DELAY_MS 5000

void long_delay_ms(uint16_t ms) {
  for(ms /= 10; ms>0; ms--) _delay_ms(10);
}

// write "high" to pin LED onm port B
#define LED_HI do { PORTB |= (1<<LED); } while(0)
// write digital "low" to pin <pn>
#define LED_LO do { PORTB &= ~(1<<LED); } while(0)
// swap that value
#define LED_XCHG do { PORTB ^= (1<<LED); } while(0)

int main(void)
{
  // data direction register for port B to output
  DDRB |= (1 << LED);
  LED_LO;

  for (;;) {
    LED_XCHG;
    long_delay_ms(DELAY_MS);
  }

  return 42;
}

