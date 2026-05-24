#ifndef TIMER_H
#define TIMER_H

#include <types.h>

void timer_init();
void timer_handler();

void timer_set_interval(uint64_t interval);

#endif // TIMER_H