#include "contiki.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "contiki-net.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "lib/random.h"

#include "node-id.h"

#include "dev/leds.h"
#include "dev/cc2420.h"

#include "tree-aggregator.h"

#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "unique-array.h"
 
static const clock_time_t ROUND_LENGTH = 10 * 60 * CLOCK_SECOND;

typedef struct
{
	uint8_t round_count;
	unique_array_t list;
} aggregation_data_t;


PROCESS(data_gather, "Data Gather");
AUTOSTART_PROCESSES(&data_gather);

static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source)
{

}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{

}

static void tree_aggregate_update(void * voiddata, void const * to_apply)
{

}

// Add our own one hop data to the list
static void tree_aggregate_own(void * ptr)
{

}

static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{

}

static void tree_agg_write_data_to_packet(tree_agg_conn_t * conn)
{

}

static tree_agg_conn_t aggconn;
static const tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};

PROCESS_THREAD(data_gather, ev, data)
{
	static rimeaddr_t sink;
	static struct etimer et;

	PROCESS_BEGIN();

#ifdef NODE_ID
	node_id_burn(NODE_ID); //Burn the Node ID to the device
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL); //Set the power levels for the radio
#endif 

	//Assign the sink node, default as 1.0
	sink.u8[0] = 1;
	sink.u8[1] = 0;

	if (rimeaddr_cmp(&rimeaddr_node_addr,&sink))
	{
		printf("We are sink node.\n");
	}

	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	// Wait for some time to let process start up - Might not be needed
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	printf("Starting Tree Aggregation\n");
	tree_agg_open(&aggconn, &sink, 118, 132, sizeof(aggregation_data_t), &callbacks);

	PROCESS_END();
}