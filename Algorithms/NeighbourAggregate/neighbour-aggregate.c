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
#include "neighbour-detect.h"

#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "unique-array.h"

// The neighbours the current node has
// This is a list of rimeaddr_t
static unique_array_t one_hop_neighbours;

typedef struct
{
	// Number of address pairs
	uint8_t length;
	uint8_t round_count;
} collect_msg_t;

typedef struct
{
	uint8_t round_count;
	unique_array_t list;
} aggregation_data_t;

typedef struct
{
	rimeaddr_t first;
	rimeaddr_t second;
} rimeaddr_pair_t;

static bool rimeaddr_pair_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	rimeaddr_pair_t const * lp = (rimeaddr_pair_t const *)left;
	rimeaddr_pair_t const * rp = (rimeaddr_pair_t const *)right;

	return
		(rimeaddr_cmp(&lp->first, &rp->first) && rimeaddr_cmp(&lp->second, &rp->second)) ||
		(rimeaddr_cmp(&lp->second, &rp->first) && rimeaddr_cmp(&lp->first, &rp->second));
}

static bool rimeaddr_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	return rimeaddr_cmp((rimeaddr_t const *)left, (rimeaddr_t const *)right);
}


static void print_ua_rimeaddr_pair(unique_array_t const * data)
{
	printf("{");

	char firstaddr[RIMEADDR_STRING_LENGTH];
	char secondaddr[RIMEADDR_STRING_LENGTH];

	unique_array_elem_t elem;
	for (elem = unique_array_first(data); unique_array_continue(data, elem); elem = unique_array_next(elem))
	{
		rimeaddr_pair_t * pair = (rimeaddr_pair_t *) unique_array_data(data, elem);

		printf("(%s, %s) ",
			addr2str_r(&pair->first, firstaddr, RIMEADDR_STRING_LENGTH),
			addr2str_r(&pair->second, secondaddr, RIMEADDR_STRING_LENGTH)
		);
	}

	printf("}\n");
}



// Data is a list of rimeaddr_t
static void list_to_array_single(unique_array_t * data, collect_msg_t * msg)
{
	rimeaddr_pair_t * addr_arr = (rimeaddr_pair_t *)(msg + 1);

	msg->length = unique_array_length(data);
	
	size_t i = 0;
	unique_array_elem_t elem;
	for (elem = unique_array_first(data); unique_array_continue(data, elem); elem = unique_array_next(elem))
	{
		rimeaddr_t * addr = (rimeaddr_t *) unique_array_data(data, elem);
		
		rimeaddr_copy(&addr_arr[i].first, &rimeaddr_node_addr);
		rimeaddr_copy(&addr_arr[i].second, addr);
		
		++i;
	}
}


PROCESS(neighbour_agg_process, "Neighbour Agg process");
PROCESS(neighbour_agg_send_data_process, "SEND DATA");
PROCESS(power_checker, "Power checker");

AUTOSTART_PROCESSES(&neighbour_agg_process,&power_checker);


static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	printf("Tree Agg: Recv\n");

	collect_msg_t const * msg = (collect_msg_t const *)packetbuf_dataptr();

	unsigned int length = msg->length;

	rimeaddr_pair_t const * neighbours = (rimeaddr_pair_t const *)(msg + 1);

	printf("R=%d {", msg->round_count);

	char firstaddr[RIMEADDR_STRING_LENGTH];
	char secondaddr[RIMEADDR_STRING_LENGTH];

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		printf("(%s,%s) ",
			addr2str_r(&neighbours[i].first, firstaddr, RIMEADDR_STRING_LENGTH),
			addr2str_r(&neighbours[i].second, secondaddr, RIMEADDR_STRING_LENGTH)
		);
	}

	printf("}\n");
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	printf("Tree Agg: Setup finsihed\n");

	if (tree_agg_is_leaf(conn))
	{
		printf("Tree Agg: Is leaf starting data aggregation\n");

		leds_on(LEDS_RED);

		process_start(&neighbour_agg_send_data_process, NULL);
	}
}

static void tree_aggregate_update(void * voiddata, void const * to_apply)
{
	toggle_led_for(LEDS_RED, CLOCK_SECOND);

	printf("Tree Agg: Update local data\n");

	unique_array_t * data = &((aggregation_data_t *)voiddata)->list;
	collect_msg_t const * data_to_apply = (collect_msg_t const *)to_apply;
	
	rimeaddr_t const * ap = (rimeaddr_t const *)(data_to_apply + 1);

	unsigned int i;
	for (i = 0; i < data_to_apply->length; i += 2)
	{
		// Check if the address pair is the the array before
		// allocating memory
		if (!unique_array_contains(data, &ap[i]))
		{
			rimeaddr_pair_t * pair = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
			rimeaddr_copy(&pair->first, &ap[i]);
			rimeaddr_copy(&pair->second, &ap[i+1]);

			unique_array_append(data, pair);
		}
	}
}

// Add our own one hop data to the list
static void tree_aggregate_own(void * ptr)
{
	printf("Tree Agg: Update local data with own data\n");

	unique_array_t * conn_list = &((aggregation_data_t *)ptr)->list;

	unique_array_elem_t elem;
	for (elem = unique_array_first(&one_hop_neighbours); unique_array_continue(&one_hop_neighbours, elem); elem = unique_array_next(elem))
	{
		rimeaddr_t * to = (rimeaddr_t *) unique_array_data(&one_hop_neighbours, elem);

		// Allocate a static pair to avoid doing malloc
		// if the pair is already in the list
		rimeaddr_pair_t pair;
		rimeaddr_copy(&pair.first, &rimeaddr_node_addr);
		rimeaddr_copy(&pair.second, to);

		if (!unique_array_contains(conn_list, &pair))
		{
			rimeaddr_pair_t * mempair = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
			memcpy(mempair, &pair, sizeof(rimeaddr_pair_t));

			unique_array_append(conn_list, mempair);
		}
	}
}

static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	printf("Tree Agg: Store packet\n");

	collect_msg_t const * msg = (collect_msg_t const *)packet;
	
	rimeaddr_pair_t const * neighbours = (rimeaddr_pair_t const *)(msg + 1);
	unsigned int neighbours_count = (length - sizeof(collect_msg_t)) / sizeof(rimeaddr_pair_t);

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;

	conn_data->round_count = msg->round_count;

	unique_array_init(&conn_data->list, &rimeaddr_pair_equality, &free);

	unsigned int i;
	for (i = 0; i < neighbours_count; ++i)
	{
		// Allocate memory for this pair
		// As we expect the data we received to be free of duplicates
		// We will not perform duplicate checks here.
		rimeaddr_pair_t * pair = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
		rimeaddr_copy(&pair->first, &neighbours[i].first);
		rimeaddr_copy(&pair->second, &neighbours[i].second);

		unique_array_append(&conn_data->list, pair);
	}
}

static void tree_agg_write_data_to_packet(tree_agg_conn_t * conn)
{
	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);

	aggregation_data_t * data_array = (aggregation_data_t *)conn->data;

	printf("Writing: ");
	print_ua_rimeaddr_pair(&data_array->list);

	unsigned int length = unique_array_length(&data_array->list);
	unsigned int packet_length = sizeof(collect_msg_t) + sizeof(rimeaddr_pair_t) * length;

	packetbuf_clear();
	packetbuf_set_datalen(packet_length);
	debug_packet_size(packet_length);

	collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
	msg->length = length;
	msg->round_count = data_array->round_count;

	rimeaddr_pair_t * msgpairs = (rimeaddr_pair_t *)(msg + 1);

	unsigned int i = 0;
	unique_array_elem_t elem;
	for (elem = unique_array_first(&data_array->list); unique_array_continue(&data_array->list, elem); elem = unique_array_next(elem))
	{
		rimeaddr_pair_t const * to = (rimeaddr_pair_t *)unique_array_data(&data_array->list, elem);

		rimeaddr_copy(&msgpairs[i].first, &to->first);
		rimeaddr_copy(&msgpairs[i].second, &to->second);

		++i;
	}

	// Free the data here
	unique_array_clear(&data_array->list);
}


static tree_agg_conn_t aggconn;
static tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};

PROCESS_THREAD(power_checker,ev,data)
{
	static struct etimer et;

	PROCESS_BEGIN();

    while (1)
    {
        etimer_set(&et, 10 * CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        printf("Powerlevel: %d\n",cc2420_get_txpower());
    }

	PROCESS_END();
}

PROCESS_THREAD(neighbour_agg_process, ev, data)
{
	static rimeaddr_t sink;
	static struct etimer et;

	PROCESS_BEGIN();

#ifdef NODE_ID
	node_id_burn(NODE_ID);
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL);
#endif

	sink.u8[0] = 1;
	sink.u8[1] = 0;
	
	if (rimeaddr_cmp(&rimeaddr_node_addr,&sink))
	{
		printf("We are sink node.\n");
	}
	
	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	printf("Setting up aggregation tree...\n");

	unique_array_init(&one_hop_neighbours, &rimeaddr_equality, &free);

	start_neighbour_detect(&one_hop_neighbours, 150);
	
	// Wait for some time to collectl neighbour info
	etimer_set(&et, 2 * 60 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	//stop_neighbour_detect();

	tree_agg_open(&aggconn, &sink, 118, 132, sizeof(aggregation_data_t), &callbacks);

	PROCESS_END();
}

PROCESS_THREAD(neighbour_agg_send_data_process, ev, data)
{
	static struct etimer et;
	static uint8_t round_count;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	printf("Starting data generation process\n");

	leds_on(LEDS_GREEN);

	round_count = 0;

	while (true)
	{
		etimer_set(&et, 10 * 60 * CLOCK_SECOND);

		if (tree_agg_is_leaf(&aggconn))
		{
			printf("Is leaf sending 1-hop data onwards\n");

			// By this point the tree should be set up,
			// so now we should move to aggregating data
			// through the tree
		 
			unsigned int one_hop_n_size = unique_array_length(&one_hop_neighbours);

			// Create the data message that we are going to send
			packetbuf_clear();
			packetbuf_set_datalen(sizeof(collect_msg_t) + one_hop_n_size * sizeof(rimeaddr_pair_t));
			debug_packet_size(sizeof(collect_msg_t) + one_hop_n_size * sizeof(rimeaddr_pair_t));
			collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
			memset(msg, 0, sizeof(collect_msg_t) + one_hop_n_size * sizeof(rimeaddr_pair_t));

			msg->round_count = round_count;

			list_to_array_single(&one_hop_neighbours, msg);
	
			tree_agg_send(&aggconn);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		++round_count;
	}

exit:
	unique_array_clear(&one_hop_neighbours);
	tree_agg_close(&aggconn);
	PROCESS_END();
}

