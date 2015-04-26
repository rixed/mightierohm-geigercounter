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
    struct event *e = next_event;
    if (unlikely_(e->delay >= MAX_SLEEP)) {
        e->delay -= MAX_SLEEP;
    } else {
        next_event = e->next;
        e->cb(e); // Beware: might call event_register, ie. update e and next_event
    }
}

// caller must have cleared Interrupt flag
// TCNT1 was 0 at the start of last event
static void event_program_timer(void)
{
    struct event *e = next_event;
    while (e) {
        if (unlikely_(e->delay >= MAX_SLEEP)) {
            OCR1A = MAX_SLEEP;
            break;
        // else we know delay fits within 16 bits:
        } else if (likely_((uint16_t)e->delay > TCNT1 + MIN_DELAY)) {
            // We assume above that TCNT1 and MIN_DELAY are small enough not
            // to make the add overflow
            OCR1A = e->delay;
            break;
        }
        // 32 cycles up to cb() call
        OCR1A = 65535U; // to allow TCNT1 to go as high as we want
        while (TCNT1 + (32U/TIMER1_PRESCALER) < (uint16_t)e->delay) {
            // busy-wait here
        }
        event_run_next();
        e = next_event;
    };
}

// This must be reentrant.
void event_register(struct event *e, uint32_t delay)
{
    struct event *next = next_event;
    struct event *prev = NULL;
    uint8_t const saved_sregs = SREG;
    cli();
    // Insert this event in the list of future events
    while (1) {
        if (! next || delay <= next->delay) {
            e->delay = delay;
            e->next = next;
            if (next) next->delay -= delay;
            if (! prev) {
                // we add the next event
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
            } else {
                prev->next = e;
            }
            break;
        }
        delay -= next->delay;
        prev = next;
        next = next->next;
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

extern inline void event_ctor(struct event *ev, void (*cb)(struct event *));

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
