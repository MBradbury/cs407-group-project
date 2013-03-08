#include "neighbour-aggregate.h"

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

#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "containers/unique-array.h"


static const clock_time_t ROUND_LENGTH = 10 * 60 * CLOCK_SECOND;
static const clock_time_t INITIAL_NEIGHBOUR_DETECT_PERIOD = 2 * 60 * CLOCK_SECOND;

static void neighbour_agg_send_data(void * ptr);

/* Gets the pointer to the main connection struct, from the given tree_agg_conn_t */
static inline neighbour_agg_conn_t * conncvt_treeconn(tree_agg_conn_t * conn)
{
	return (neighbour_agg_conn_t *)conn;
}

/* Gets the pointer to the main connection struct, from the given neighbour_detect_conn_t */
static inline neighbour_agg_conn_t * conncvt_neighbourdetect_conn(neighbour_detect_conn_t * conn)
{
	return (neighbour_agg_conn_t *)
	(((char *)conn) - sizeof(tree_agg_conn_t));
}

typedef struct
{
	// Number of address pairs
	unsigned int length;
	unsigned int round_count;
} collect_msg_t;

typedef struct
{
	unsigned int round_count;
	unique_array_t list;
} aggregation_data_t;


#ifndef NDEBUG
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
#endif



// Data is a list of rimeaddr_t
static size_t list_to_array_single(unique_array_t * data, collect_msg_t * msg)
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

	return i;
}

static void neighbour_agg_round_complete(neighbour_detect_conn_t * conn, unique_array_t * neighbours, uint16_t round_count)
{	
	//get main conn, for the array of one_hop_neighbours
	neighbour_agg_conn_t * mainconn = conncvt_neighbourdetect_conn(conn);
	
	//Empty current one_hop_neighbours
	unique_array_clear(&mainconn->one_hop_neighbours);
	
	//copy new array into old
	unique_array_merge(&mainconn->one_hop_neighbours, neighbours);
}

static void tree_agg_recv(tree_agg_conn_t * tconn, rimeaddr_t const * source, void const * packet, unsigned int packet_length)
{
	neighbour_agg_conn_t * conn = conncvt_treeconn(tconn);

	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	printf("Neighbour Agg - Neighbour Aggregate: Recv\n");

	collect_msg_t const * msg = (collect_msg_t const *)packet;

	unsigned int length = msg->length;

	rimeaddr_pair_t const * neighbours = (rimeaddr_pair_t const *)(msg + 1);

	printf("R=%d|", msg->round_count);

	char firstaddr[RIMEADDR_STRING_LENGTH];
	char secondaddr[RIMEADDR_STRING_LENGTH];

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		printf("%s,%s",
			addr2str_r(&neighbours[i].first, firstaddr, RIMEADDR_STRING_LENGTH),
			addr2str_r(&neighbours[i].second, secondaddr, RIMEADDR_STRING_LENGTH)
		);

		if (i + 1 < length)
		{
			printf("~");
		}
	}
	
	printf("\n");
	
	// Call the callback
	(*conn->callbacks.data_callback_fn)(neighbours, length, msg->round_count);
}

static void tree_agg_setup_finished(tree_agg_conn_t * tconn)
{
	neighbour_agg_conn_t * conn = conncvt_treeconn(tconn);

	printf("Neighbour Agg: Setup finsihed conn: %p\n",conn);

	if (tree_agg_is_leaf(tconn))
	{
		printf("Neighbour Agg: Is leaf starting data aggregation\n");

		leds_on(LEDS_GREEN);

		// Start sending data if we are a leaf
		neighbour_agg_send_data(conn);
	}
}

static void tree_aggregate_update(void * voiddata, void const * to_apply, unsigned int length)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	printf("Neighbour Agg: Update local data\n");

	unique_array_t * data = &((aggregation_data_t *)voiddata)->list;
	collect_msg_t const * data_to_apply = (collect_msg_t const *)to_apply;
	
	rimeaddr_pair_t const * ap = (rimeaddr_pair_t const *)(data_to_apply + 1);

	unsigned int i;
	for (i = 0; i < data_to_apply->length; ++i)
	{
		// Check if the address pair is the the array before
		// allocating memory
		if (!unique_array_contains(data, &ap[i]))
		{
			rimeaddr_pair_t * pair = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
			rimeaddr_copy(&pair->first, &ap[i].first);
			rimeaddr_copy(&pair->second, &ap[i].second);

			unique_array_append(data, pair);
		}
	}
}

// Add our own one hop data to the list
static void tree_aggregate_own(void * ptr)
{
	printf("Neighbour Agg: Update local data with own data\n");
	
	neighbour_agg_conn_t * conn = conncvt_treeconn(ptr);

	unique_array_t * conn_list = &((aggregation_data_t *)ptr)->list;

	unique_array_elem_t elem;
	for (elem = unique_array_first(&conn->one_hop_neighbours); 
		unique_array_continue(&conn->one_hop_neighbours, elem); 
		elem = unique_array_next(elem))
	{
		rimeaddr_t * to = (rimeaddr_t *) unique_array_data(&conn->one_hop_neighbours, elem);

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

static void tree_agg_store_packet(tree_agg_conn_t * tconn, void const * packet, unsigned int length)
{
	neighbour_agg_conn_t * conn = conncvt_treeconn(tconn);

	printf("Neighbour Agg: Store packet\n");

	collect_msg_t const * msg = (collect_msg_t const *)packet;
	
	rimeaddr_pair_t const * neighbours = (rimeaddr_pair_t const *)(msg + 1);
	unsigned int neighbours_count = (length - sizeof(collect_msg_t)) / sizeof(rimeaddr_pair_t);

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->tc.data;

	conn_data->round_count = msg->round_count;

	unique_array_init(&conn_data->list, &rimeaddr_pair_equality, &free);

	unsigned int i;
	for (i = 0; i < neighbours_count; ++i)
	{
		// Allocate memory for this pair
		// As we expect the data we received to be free of duplicates
		// We will not perform duplicate checks here.
		rimeaddr_pair_t * pair = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
		memcpy(pair, &neighbours[i], sizeof(rimeaddr_pair_t));

		unique_array_append(&conn_data->list, pair);
	}
}

static void tree_agg_write_data_to_packet(tree_agg_conn_t * tconn, void ** data, size_t * data_length)
{
	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);

	aggregation_data_t * data_array = (aggregation_data_t *)tconn->data;

#ifndef NDEBUG
	printf("Neighbour Agg: Writing: ");
	print_ua_rimeaddr_pair(&data_array->list);
#endif

	unsigned int ulist_length = unique_array_length(&data_array->list);

	*data_length = sizeof(collect_msg_t) + sizeof(rimeaddr_pair_t) * ulist_length;
	*data = malloc(*data_length);

	collect_msg_t * msg = (collect_msg_t *)*data;
	msg->length = ulist_length;
	msg->round_count = data_array->round_count;

	rimeaddr_pair_t * msgpairs = (rimeaddr_pair_t *)(msg + 1);

	unsigned int i = 0;
	unique_array_elem_t elem;
	for (elem = unique_array_first(&data_array->list); 
		unique_array_continue(&data_array->list, elem); 
		elem = unique_array_next(elem))
	{
		rimeaddr_pair_t const * from = (rimeaddr_pair_t *)unique_array_data(&data_array->list, elem);
		memcpy(&msgpairs[i], from, sizeof(rimeaddr_pair_t));

		++i;
	}

	// Free the data here
	unique_array_clear(&data_array->list);
}


static const tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};


static void neighbour_agg_send_data(void * ptr)
{
	printf("Neighbour Agg send data leaf\n");

	// Extract the struct
	neighbour_agg_conn_t * conn = (neighbour_agg_conn_t *)ptr;

	// Process the data, and send it on
	if (tree_agg_is_leaf(&conn->tc))
	{
		// By this point the tree should be set up,
		// so now we should move to aggregating data
		// through the tree
	 
		unsigned int one_hop_n_size = unique_array_length(&conn->one_hop_neighbours);

		// Create the data message that we are going to send
		size_t message_length = sizeof(collect_msg_t) + one_hop_n_size * sizeof(rimeaddr_pair_t);
		collect_msg_t * msg = (collect_msg_t *)malloc(message_length);
		memset(msg, 0, message_length);

		msg->round_count = conn->round_count;

		list_to_array_single(&conn->one_hop_neighbours, msg);

		printf("Neighbour Agg: Is leaf sending 1-hop data onwards length:%d |1HN|:%d\n", message_length, one_hop_n_size);

		tree_agg_send(&conn->tc, msg, message_length);
	}

	conn->round_count++;
	
	// Setup a ctimer to repeat in ROUND_LENGTH time
	ctimer_set(&conn->ct_send_data, ROUND_LENGTH, &neighbour_agg_send_data, conn);
}

typedef struct
{
	neighbour_agg_conn_t * conn;
	rimeaddr_t const * sink;
	uint16_t ch1;
	uint16_t ch2;
} open_tree_agg_t;

// Called by the ctimer after the initial setup
static void open_tree_agg(void * ptr)
{
	open_tree_agg_t * data = (open_tree_agg_t *)ptr; 

	tree_agg_open(&data->conn->tc, data->sink, data->ch1, data->ch2, sizeof(aggregation_data_t), &callbacks);
	free(data);
}

static neighbour_detect_callbacks_t neighbour_detect_callbacks = {&neighbour_agg_round_complete};

bool neighbour_aggregate_open(neighbour_agg_conn_t * conn,
	rimeaddr_t const * sink,
	uint16_t ch1, uint16_t ch2, uint16_t ch3,
	neighbour_agg_callbacks_t const * callback_fns)
{
	if (conn != NULL && ch1 != 0 && ch2 != 0 && ch3 != 0 && callback_fns != NULL)
	{
		// We need to set the random number generator here
		random_init(*(uint16_t*)(&rimeaddr_node_addr));

		printf("Neighbour Agg: Setting up aggregation tree\n");

		if (!unique_array_init(&conn->one_hop_neighbours, &rimeaddr_equality, &free))
		{
			return false;
		}

		memcpy(&conn->callbacks, callback_fns, sizeof(neighbour_agg_callbacks_t)); //copy in the callbacks

		start_neighbour_detect(&conn->nd, ch3, &neighbour_detect_callbacks);
	
		open_tree_agg_t * s_init = (open_tree_agg_t *) malloc(sizeof(open_tree_agg_t));

		// Wait for some time to collect neighbour info, and open the tree
		s_init->conn = conn;
		s_init->sink = sink;
		s_init->ch1 = ch1;
		s_init->ch2 = ch2;

		ctimer_set(&conn->ct_initial_wait, INITIAL_NEIGHBOUR_DETECT_PERIOD, &open_tree_agg, s_init);

		return true;
	}
	
	return false;
}

void neighbour_aggregate_close(neighbour_agg_conn_t * conn)
{
	if (conn != NULL)
	{
		// Close the connections
		ctimer_stop(&conn->ct_send_data);
		ctimer_stop(&conn->ct_initial_wait);
		unique_array_clear(&conn->one_hop_neighbours);
		tree_agg_close(&conn->tc);
		conn->round_count = 0;
	}
}

#ifdef NEIGHBOUR_AGG_APPLICATION

PROCESS(neighbour_agg_process, "Neighbour Agg process");

AUTOSTART_PROCESSES(&neighbour_agg_process);

static void handle_neighbour_data(rimeaddr_pair_t const * pairs, unsigned int length, unsigned int round_count)
{
	printf("Got some data of length:%d in round %d\n", length, round_count);
}

static neighbour_agg_conn_t nconn;

static const neighbour_agg_callbacks_t c = {&handle_neighbour_data};

PROCESS_THREAD(neighbour_agg_process, ev, data)
{
	static rimeaddr_t sink;

	PROCESS_BEGIN();

#ifdef NODE_ID
	node_id_burn(NODE_ID);
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL);
#endif

	sink.u8[0] = 1;
	sink.u8[1] = 0;
	
	if (rimeaddr_cmp(&rimeaddr_node_addr, &sink))
	{
		printf("We are sink node.\n");
	}
	
	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	printf("Setting up aggregation tree...\n");

	neighbour_aggregate_open(&nconn, &sink, 121, 110, 150, &c);

	PROCESS_END();
}

#endif
