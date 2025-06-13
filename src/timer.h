#ifndef TIMER_H
#define TIMER_H

#include <time.h> // For struct timespec

void cache_timer_start(struct timespec *start_time);
int cache_timer_end(const struct timespec *start_time);

void command_timer_start(struct timespec *start_time);
double command_timer_end(const struct timespec *start_time);

#endif // ZU_TIMER_H
