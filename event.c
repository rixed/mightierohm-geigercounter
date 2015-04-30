// Event library build on TIMER1
#include <stdlib.h>
#include <limits.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include "miscmacs.h"
#include "event.h"
#include "cpp.h"

static struct event *volatile next_event = NULL;

#define MIN_DELAY 64U // above this we return the CPU, below this we wait here
#define MAX_SLEEP (65535U-MIN_DELAY)

static void event_run_next(void)
{
  TCNT1 = 0;
  struct event *const e = next_event;
  if (unlikely_(e->delay >= MAX_SLEEP)) {
    e->delay -= MAX_SLEEP;
  } else {
    void (*cb)(struct event *) = e->cb;
    e->cb = NULL;
    next_event = e->next;
    cb(e); // Beware: might call event_register, ie. update next_event
  }
}

// caller must have cleared Interrupt flag
// TCNT1 was 0 at the start of last event
static void event_program_timer(void)
{
  struct event *const e = next_event;
  if (! e) return;

  if (unlikely_(e->delay >= MAX_SLEEP)) {
    OCR1A = MAX_SLEEP;
  } else {
    uint16_t close = TCNT1 + MIN_DELAY;
    if (likely_((uint16_t)e->delay > close)) {
      OCR1A = (uint16_t)e->delay;
    } else {  // now is either after or very close to e->delay
      OCR1A = close;
    }
  }
}

// This must be reentrant.
// Also, e may already been connected (if it has not fired yet).
void event_register(struct event *e, void (*cb)(struct event *), uint32_t delay)
{
  uint8_t const saved_sregs = SREG;
  cli();
  // next_event is not volatile any more

  BIT_SET(PORTB, PB4);
  // enqueue this task if it was queued
  if (e->cb) {
    struct event **ee;
    for (ee = (struct event **)&next_event; *ee != e; ee = &(*ee)->next) ;
    *ee = (*ee)->next;
  }
  BIT_CLEAR(PORTB, PB4);
  e->cb = cb;

  struct event *next = next_event;
  struct event *prev = NULL;

  // Insert this event in the list of future events
  while (next && delay > next->delay) {
    delay -= next->delay;
    prev = next;
    next = next->next;
  }

  e->delay = delay;
  e->next = next;
  if (next) {
    next->delay -= delay;
  }
  if (prev) {
    prev->next = e;
  } else {
    // we add the first event
    next_event = e;
    if (next) {
      uint16_t const now = TCNT1;
      if (now < next->delay) {
        next->delay -= now;
      } else {
        next->delay = 0; // Should seldom happen :-(
      }
    }
    TCNT1 = 0;
    event_program_timer();
  }
  SREG = saved_sregs;
}

// First version : blocking ISR (TODO: allow interrupts while in cb())
ISR(TIMER1_COMPA_vect)
{
    if (next_event) {
        event_run_next();
        event_program_timer();
    }
}

extern inline void event_ctor(struct event *ev);

void event_init(void)
{
    /* The Timer/Counter Control Registers (TCCR1A/B) are 8-bit registers
       and have no CPU access restrictions.
       TCCR1A bits: COM1A1,COM1A0,COM1B1,COM1B0,-,-,WGM11,WGM10
       TCCR1B bits: ICNC1,ICES1,-,WGM13,WGM12,CS12,CS11,CS10

       We choose the Clear Timer on Compare Match (CTC) mode of operation,
       In this mode, the interrupt will be executed (and counter cleared)
       when counter reach OCR1A (if WGM=4) or ICR1 (WGM=12) register. Here
       we go for OCR1A.
    */

    // For some reason you must write OCR1A *after* TCCR1A/B
    // (otherwise writing TCCR1A/B reset OCR1A).
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11);

    BIT_SET(TIFR, OCF1A);   // clear any pending interrupts
    BIT_SET(TIMSK, OCIE1A); // enable timer int
}
