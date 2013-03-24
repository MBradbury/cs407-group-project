#ifndef CS407_NEIGHBOUR_DETECT
#define CS407_NEIGHBOUR_DETECT

#include "contiki.h"

#include "containers/unique-array.h"
#include "containers/map.h"

#include "net/rime/neighbor-discovery.h"

#include <stdint.h>
#include <stdbool.h>

struct neighbour_detect_conn_t;

typedef struct
{
	//called after a round has been completed, with the latest results for a round
    void (* round_complete_callback)(struct neighbour_detect_conn_t * conn, unique_array_t * neighbours, uint16_t round_count);
} neighbour_detect_callbacks_t;

typedef struct neighbour_detect_conn_t
{
	struct neighbor_discovery_conn nd;

	unique_array_t results;

	neighbour_detect_callbacks_t const * callbacks;
	
	map_t round_map;

	struct ctimer round_timer;
	
	uint16_t round_count;
	uint16_t missed_round_threshold;

} neighbour_detect_conn_t;

bool start_neighbour_detect(neighbour_detect_conn_t * conn,
	uint16_t channel, neighbour_detect_callbacks_t const * cb_fns,
	clock_time_t initial_interval, clock_time_t min_interval, clock_time_t max_interval,
	clock_time_t round_time, uint16_t missed_round_threshold);

void stop_neighbour_detect(neighbour_detect_conn_t * conn);

#endif /*CS407_NEIGHBOUR_DETECT*/
