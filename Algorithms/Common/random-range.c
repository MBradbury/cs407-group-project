#include "random-range.h"

#include <stdint.h>

#include "lib/random.h"

clock_time_t random_time(unsigned int from, unsigned int to, double granularity)
{
	double random = granularity * ((random_rand() % (unsigned int)(from / granularity)) + (unsigned int)(to / granularity));

	return (clock_time_t)(random * CLOCK_SECOND);
}

double random_range_double(double from, double to)
{
	double range = to - from;
	double d = UINT16_MAX / range;

	return from + (random_rand() / d);
}
