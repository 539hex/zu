#ifndef TIMER_H
#define TIMER_H

#include <time.h> // For struct timespec

void zu_timer_start(struct timespec *start_time);
double zu_timer_end(const struct timespec *start_time);

#endif // ZU_TIMER_H
