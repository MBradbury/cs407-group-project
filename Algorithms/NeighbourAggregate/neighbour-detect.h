#ifndef CS407_NEIGHBOUR_DETECT
#define CS407_NEIGHBOUR_DETECT

#include "unique-array.h"
#include "map.h"

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

	neighbour_detect_callbacks_t callbacks;
	
	map_t round_map;
	struct ctimer round_timer;
	uint16_t round_count;

} neighbour_detect_conn_t;

bool start_neighbour_detect(neighbour_detect_conn_t * conn, uint16_t channel, neighbour_detect_callbacks_t const * cb_fns);
void stop_neighbour_detect(neighbour_detect_conn_t * conn);

#endif /*CS407_NEIGHBOUR_DETECT*/
