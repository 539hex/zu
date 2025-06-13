#include "timer.h"

void command_timer_start(struct timespec *start_time)
{
    clock_gettime(CLOCK_MONOTONIC, start_time);
}

double command_timer_end(const struct timespec *start_time)
{
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long seconds = end_time.tv_sec - start_time->tv_sec;
    long nanoseconds = end_time.tv_nsec - start_time->tv_nsec;
    // Handle the case where nanoseconds is negative
    if (nanoseconds < 0)
    {
        seconds--;
        nanoseconds += 1000000000;
    }
    return (double)seconds * 1000.0 + (double)nanoseconds / 1000000.0;
}

void cache_timer_start(struct timespec *start_time)
{
    clock_gettime(CLOCK_MONOTONIC, start_time);
}

int cache_timer_end(const struct timespec *start_time)
{
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    int seconds = end_time.tv_sec - start_time->tv_sec;
    return seconds;
}