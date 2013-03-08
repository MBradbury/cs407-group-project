#include "neighbour-detect.h"

#include "contiki.h"
#include "net/rime.h"
#include "net/rime/neighbor-discovery.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "map.h"

#include "led-helper.h"
#include "debug-helper.h"

#define INITIAL_INTERVAL (15 * CLOCK_SECOND)
#define MIN_INTERVAL (5 * CLOCK_SECOND)
#define MAX_INTERVAL (120 * CLOCK_SECOND)

#define ROUND_TIME (MAX_INTERVAL * 2)
#define MISSED_ROUND_THRESHOLD 4

/* Gets the pointer to the main connection struct, from the given tree_agg_conn_t */
static inline neighbour_detect_conn_t * conncvt_detectconn(struct neighbor_discovery_conn * conn)
{
	return (neighbour_detect_conn_t *)conn;
}

static bool rimeaddr_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	return rimeaddr_cmp((rimeaddr_t const *)left, (rimeaddr_t const *)right);
}

typedef struct
{
	rimeaddr_t node;
	uint16_t round_last_seen;
} round_map_elem_t;
 

static void round_complete(void * ptr)
{
	neighbour_detect_conn_t * conn = (neighbour_detect_conn_t *)ptr;

	// We want to remove any nodes we haven't seen in a while
	unique_array_elem_t elem;
	for (elem = unique_array_first(&conn->results); unique_array_continue(&conn->results, elem); )
	{
		rimeaddr_t * addr = (rimeaddr_t *) unique_array_data(&conn->results, elem);

		round_map_elem_t * stored = (round_map_elem_t *)map_get(&conn->round_map, addr);

		if (conn->round_count - stored->round_last_seen >= MISSED_ROUND_THRESHOLD)
		{
			printf("Detected that %s is no longer in our neighbourhood, so removing it (R=%u RLS=%u)\n",
				addr2str(addr), conn->round_count, stored->round_last_seen);

			// We haven't seen this node for a while, so need to remove it
			// from both the results and our local map
			map_remove(&conn->round_map, addr);
			unique_array_remove(&conn->results, elem);
		}
		else
		{
			elem = unique_array_next(elem);
		}
	}

	++conn->round_count;

	//call the callback
	(*conn->callbacks.round_complete_callback)(conn, &conn->results, conn->round_count);

	neighbor_discovery_set_val(&conn->nd, conn->round_count);

	// Reset the timer
	ctimer_set(&conn->round_timer, ROUND_TIME, &round_complete, conn);
}


static void neighbor_discovery_recv(struct neighbor_discovery_conn * c, rimeaddr_t const * from, uint16_t value)
{
	//printf("Mote With Address: %s is my Neighbour ", addr2str(from));
	//printf(" on node: %s\n", addr2str(&rimeaddr_node_addr));

	neighbour_detect_conn_t * conn = conncvt_detectconn(c);

	if (!unique_array_contains(&conn->results, from))
	{
		rimeaddr_t * store = (rimeaddr_t *)malloc(sizeof(rimeaddr_t));
		rimeaddr_copy(store, from);
		unique_array_append(&conn->results, store);

		printf("Recording Address: %s", addr2str(from));
		printf(" on node: %s\n", addr2str(&rimeaddr_node_addr));
	}

	// Update round map
	round_map_elem_t * stored = (round_map_elem_t *)map_get(&conn->round_map, from);

	if (stored)
	{
		// Update data
		if (value > stored->round_last_seen)
		{
			stored->round_last_seen = value;
		}
	}
	else
	{
		// Allocate memory for the data
		stored = malloc(sizeof(round_map_elem_t));
		rimeaddr_copy(&stored->node, from);
		stored->round_last_seen = value;

		// Put data in the map
		map_put(&conn->round_map, stored);
	}

	printf("Neighbour Discovery: got addr seen before from: %s\n", addr2str(from));

	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);
}

static void neighbor_discovery_sent(struct neighbor_discovery_conn * c)
{
	//printf("Neighbour Discovery: sent message\n");
}

static const struct neighbor_discovery_callbacks neighbor_discovery_callbacks =
	{ &neighbor_discovery_recv, &neighbor_discovery_sent };

bool start_neighbour_detect(neighbour_detect_conn_t * conn, uint16_t channel, neighbour_detect_callbacks_t const * cb_fns)
{
	// Check parameters
	if (conn != NULL && channel != 0 && cb_fns != NULL)
	{
		printf("Neighbour Discovery: Started!\n");

		map_init(&conn->round_map, &rimeaddr_equality, &free);

		unique_array_init(&conn->results, &rimeaddr_equality, &free);
		
		conn->round_count = 0;

		memcpy(&conn->callbacks, cb_fns, sizeof(neighbour_detect_callbacks_t)); //copy in the callbacks

		neighbor_discovery_open(
	        &conn->nd,
	        channel, 
	        INITIAL_INTERVAL,
	        MIN_INTERVAL,
	        MAX_INTERVAL,
	        &neighbor_discovery_callbacks
		);

	    neighbor_discovery_start(&conn->nd, conn->round_count);

		ctimer_set(&conn->round_timer, ROUND_TIME, &round_complete, conn);

		leds_on(LEDS_BLUE);

		return true;
	}

	return false;
}

void stop_neighbour_detect(neighbour_detect_conn_t * conn)
{
	if (conn != NULL)
	{
		printf("Neighbour Discovery: Stopped!\n");

		neighbor_discovery_close(&conn->nd);

		ctimer_stop(&conn->round_timer);

		map_clear(&conn->round_map);

		unique_array_clear(&conn->results);

		leds_off(LEDS_BLUE);
	}
}

