//#ifndef CS407_NEIGHBOUR_AGGREGATE
//#define CS407_NEIGHBOUR_AGGREGATE

#include "net/rime.h"

typedef struct
{
	rimeaddr_t first;
	rimeaddr_t second;
} rimeaddr_pair_t;

void start_neighbour_aggregate(void (* data_recieved_fn)(rimeaddr_pair_t *, unsigned int, int));

//#endif /*CS407_NEIGHBOUR_AGGREGATE*/