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

static bool rimeaddr_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	return
		rimeaddr_cmp((rimeaddr_t const *)left, (rimeaddr_t const *)right);
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

	unique_array_init(results, &rimeaddr_equality, &free);

	results_ptr = results;

	neighbor_discovery_open(
        &nd,
        channel, 
        5 * CLOCK_SECOND, 
        5 * CLOCK_SECOND, 
        120 * CLOCK_SECOND,
        &neighbor_discovery_callbacks
	);

    neighbor_discovery_start(&nd, 1);

	leds_on(LEDS_BLUE);
}

void stop_neighbour_detect(void)
{
	printf("Neighbour Discovery: Stopped!\n");

	neighbor_discovery_close(&nd);
	results_ptr = NULL;

	leds_off(LEDS_BLUE);
}

