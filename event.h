// Event library build on TIMER1
#ifndef EVENT_H_120813
#define EVENT_H_120813
#include <stdint.h>

#define TIMER1_PRESCALER 8U
#define US_TO_TIMER1_TICKS(us) (uint32_t)(((uint64_t)(us) * (F_CPU / TIMER1_PRESCALER)) / 1000000ULL)

struct event {
  uint32_t delay;
  struct event *next;
  void (*cb)(struct event *); // NULL if not scheduled
};

static inline void event_ctor(struct event *ev)
{
  ev->cb = NULL;
}

void event_register(struct event *, void (*cb)(struct event *), uint32_t delay /* in ticks */);

void event_init(void);

#endif
