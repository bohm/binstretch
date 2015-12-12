#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <sys/time.h>

// functions for measuring elapsed time
// code origin: http://stackoverflow.com/questions/1468596/calculating-elapsed-time-in-a-c-program-in-milliseconds

#ifndef _MEASURE_H
#define _MEASURE_H 1

// Global variable measuring total time spent on dynamic programming.
struct timeval dynTotal;

// Global variable measuring # of dyn. programming runs
unsigned long long int test_counter = 0;
unsigned long long int maximum_feasible_counter = 0;

// Run at the start of the program to ensure measurement initialization.
void measure_init()
{
    dynTotal.tv_sec = 0;
    dynTotal.tv_usec = 0;
}

/* Return 1 if the difference is negative, otherwise 0.  */
int timeval_subtract(struct timeval *result, const struct timeval *t2, const struct timeval *t1)
{
    long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;

    return (diff<0);
}


void timeval_add(struct timeval *result, const struct timeval *t)
{
    long long int diff = (t->tv_usec + 1000000 * t->tv_sec) + (result->tv_usec + 1000000 * result->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;
    
}

void timeval_print(const struct timeval *t)
{
    MEASURE_PRINT("%ld.%06ld", t->tv_sec, t->tv_usec);

}
#endif
