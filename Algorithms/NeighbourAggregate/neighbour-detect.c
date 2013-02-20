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

static struct neighbor_discovery_conn nd;
static unique_array_t * results_ptr = NULL;

#define INITIAL_INTERVAL (15 * CLOCK_SECOND)
#define MIN_INTERVAL (5 * CLOCK_SECOND)
#define MAX_INTERVAL (120 * CLOCK_SECOND)

static struct ctimer round_timer;
#define ROUND_TIME (MAX_INTERVAL * 2)
static uint16_t round_count = 0;
#define MISSED_ROUND_THRESHOLD 4


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

static map_t round_map;


static void round_complete(void * ptr)
{
	// We want to remove any nodes we haven't seen in a while

	unique_array_elem_t elem;
	for (elem = unique_array_first(results_ptr); unique_array_continue(results_ptr, elem); )
	{
		rimeaddr_t * addr = (rimeaddr_t *) unique_array_data(results_ptr, elem);

		round_map_elem_t * stored = (round_map_elem_t *)map_get(&round_map, addr);

		if (round_count - stored->round_last_seen >= MISSED_ROUND_THRESHOLD)
		{
			printf("Detected that %s is no longer in our neighbourhood, so removing it (R=%u RLS=%u)\n",
				addr2str(addr), round_count, stored->round_last_seen);

			// We haven't seen this node for a while, so need to remove it
			// from both the results and our local map
			map_remove(&round_map, addr);
			unique_array_remove(results_ptr, elem);
		}
		else
		{
			elem = unique_array_next(elem);
		}
	}

	++round_count;

	neighbor_discovery_set_val(&nd, round_count);

	// Reset the timer
	ctimer_set(&round_timer, ROUND_TIME, &round_complete, NULL);
}


static void neighbor_discovery_recv(struct neighbor_discovery_conn * c, rimeaddr_t const * from, uint16_t value)
{
	//printf("Mote With Address: %s is my Neighbour ", addr2str(from));
	//printf(" on node: %s\n", addr2str(&rimeaddr_node_addr));

	if (results_ptr != NULL)
	{
		if (!unique_array_contains(results_ptr, from))
		{
			rimeaddr_t * store = (rimeaddr_t *)malloc(sizeof(rimeaddr_t));
			rimeaddr_copy(store, from);
			unique_array_append(results_ptr, store);

			printf("Recording Address: %s", addr2str(from));
			printf(" on node: %s\n", addr2str(&rimeaddr_node_addr));
		}
	}

	// Update round map
	round_map_elem_t * stored = (round_map_elem_t *)map_get(&round_map, from);

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
		map_put(&round_map, stored);
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

void start_neighbour_detect(unique_array_t * results, uint16_t channel)
{
	printf("Neighbour Discovery: Started!\n");

	map_init(&round_map, &rimeaddr_equality, &free);

	results_ptr = results;

	neighbor_discovery_open(
        &nd,
        channel, 
        INITIAL_INTERVAL,
        MIN_INTERVAL,
        MAX_INTERVAL,
        &neighbor_discovery_callbacks
	);

    neighbor_discovery_start(&nd, round_count);

	ctimer_set(&round_timer, ROUND_TIME, &round_complete, NULL);

	leds_on(LEDS_BLUE);
}

void stop_neighbour_detect(void)
{
	printf("Neighbour Discovery: Stopped!\n");

	neighbor_discovery_close(&nd);
	results_ptr = NULL;

	ctimer_stop(&round_timer);

	map_clear(&round_map);

	leds_off(LEDS_BLUE);
}

