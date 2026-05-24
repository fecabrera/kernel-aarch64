#ifndef TIMER_H
#define TIMER_H

#include <types.h>

#define TIMER_INTERVAL_MS 10 // 10 ms

void timer_init();
void timer_handler();

#endif // TIMER_H