#include "random-range.h"

#include "lib/random.h"

clock_time_t random_time(unsigned int from, unsigned int to, double granularity)
{
	double random = granularity * ((random_rand() % (unsigned int)(from / granularity)) + (unsigned int)(to / granularity));

	return (clock_time_t)(random * CLOCK_SECOND);
}

