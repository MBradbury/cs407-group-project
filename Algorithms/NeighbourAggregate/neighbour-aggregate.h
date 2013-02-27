#ifndef CS407_NEIGHBOUR_AGGREGATE
#define CS407_NEIGHBOUR_AGGREGATE

#include "net/rime.h"

#include "tree-aggregator.h"
#include "neighbour-detect.h"

struct neighbour_agg_conn_t;

typedef struct
{
	rimeaddr_t first;
	rimeaddr_t second;
} rimeaddr_pair_t;

typedef struct
{
	//Called when data is received at the sink, from the tree
	void (* data_callback_fn)(rimeaddr_pair_t const * pairs, unsigned int length, int round_count);
} neighbour_agg_callbacks_t;

typedef struct neighbour_agg_conn_t
{
	tree_agg_conn_t tc;

	unique_array_t one_hop_neighbours;

	neighbour_agg_callbacks_t callbacks;

	unsigned int round_count;
} neighbour_agg_conn_t;

/* starts the neighbour aggregate the argument is a function that will be used by the sink to process the data */
void neighbour_aggregate_open(neighbour_agg_conn_t * conn, 
								uint16_t ch1,
								uint16_t ch2,
								uint16_t ch3,
								neighbour_agg_callbacks_t const * callback_fns);

void neighbour_aggregate_close(neighbour_agg_conn_t * conn);

#endif /*CS407_NEIGHBOUR_AGGREGATE*/
