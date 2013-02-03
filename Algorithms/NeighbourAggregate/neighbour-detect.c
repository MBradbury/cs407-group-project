#include "neighbour-detect.h"

#include "contiki.h"
#include "net/rime.h"
#include "net/rime/neighbor-discovery.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "led-helper.h"
#include "debug-helper.h"

static struct neighbor_discovery_conn nd;
static unique_array_t * results_ptr = NULL;

#define INITIAL_INTERVAL (25 * CLOCK_SECOND)
#define MIN_INTERVAL (15 * CLOCK_SECOND)
#define MAX_INTERVAL (120 * CLOCK_SECOND)

static struct ctimer round_timer;
#define ROUND_TIME (MAX_INTERVAL * 2)
static uint16_t round_count = 0;


static void round_complete(void * ptr)
{
	++round_count;

	neighbor_discovery_set_val(&nd, round_count);

	// Reset the timer
	ctimer_set(&round_timer, ROUND_TIME, &round_complete, NULL);
}


static void neighbor_discovery_recv(struct neighbor_discovery_conn * c, rimeaddr_t const * from, uint16_t val)
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

	printf("Neighbour Discovery: got addr seen before from: %s\n", addr2str(from));

	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);
}

static void neighbor_discovery_sent(struct neighbor_discovery_conn * c)
{
	printf("Neighbour Discovery: sent message\n");
}

static const struct neighbor_discovery_callbacks neighbor_discovery_callbacks =
	{ &neighbor_discovery_recv, &neighbor_discovery_sent };

void start_neighbour_detect(unique_array_t * results, uint16_t channel)
{
	printf("Neighbour Discovery: Started!\n");

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

	leds_off(LEDS_BLUE);
}

