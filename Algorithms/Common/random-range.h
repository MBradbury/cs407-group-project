#ifndef CS407_RANDOM_RANGE
#define CS407_RANDOM_RANGE

#include <sys/clock.h>

clock_time_t random_time(unsigned int from, unsigned int to, double granularity);

double random_range_double(double from, double to);

#endif /*CS407_RANDOM_RANGE*/

