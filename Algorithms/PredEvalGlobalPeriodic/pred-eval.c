#include "pegp.h"

#include "contiki.h"
#include "net/rime.h"

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
#include "containers/array-list.h"


#ifdef PE_DEBUG
#	define PEDPRINTF(...) printf(__VA_ARGS__)
#else
#	define PEDPRINTF(...)
#endif

#define ROUND_LENGTH ((clock_time_t) 5 * 60 * CLOCK_SECOND)
#define TRICKLE_INTERVAL (clock_time_t)(2 * CLOCK_SECOND)

#define NODE_DATA_INDEX(array, index, size) \
	(((char *)array) + ((index) * (size)))

#define CNODE_DATA_INDEX(array, index, size) \
	(((char const *)array) + ((index) * (size)))


typedef struct
{
	unsigned int round_count;

	// List of rimeaddr_t
	unique_array_t list;
} aggregation_data_t;

typedef struct
{
    uint8_t length;
    unsigned int round_count;
} collected_data_t;


static inline pegp_conn_t * conncvt_tree_agg(tree_agg_conn_t * conn)
{
	return (pegp_conn_t *)conn;
}

static inline pegp_conn_t * conncvt_neighbour_agg(neighbour_agg_conn_t * conn)
{
	return (pegp_conn_t *)
		(((char *)conn) - sizeof(tree_agg_conn_t));
}

static inline pegp_conn_t * conncvt_predicate_manager(predicate_manager_conn_t * conn)
{
	return (pegp_conn_t *)
		(((char *)conn) - sizeof(tree_agg_conn_t) - sizeof(neighbour_agg_conn_t));
}


static void handle_neighbour_data(neighbour_agg_conn_t * conn,
	rimeaddr_pair_t const * pairs, unsigned int length, unsigned int round_count)
{
	pegp_conn_t * pegp = conncvt_neighbour_agg(conn);

	PEDPRINTF("PEGP: Neighbour r=%u len=%u\n", round_count, length);

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		if (!unique_array_contains(&pegp->neighbour_info, &pairs[i]))
		{
			unique_array_append(&pegp->neighbour_info, rimeaddr_pair_clone(&pairs[i]));
		}
	}
}

PROCESS(data_evaluation_process, "Data eval");
PROCESS(send_data_process, "Send data process");

// Sink recieved final set of data
static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source, void const * packet, unsigned int packet_length)
{
	pegp_conn_t * pegp = conncvt_tree_agg(conn);

	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	// Extract data from packet buffer
	collected_data_t const * msg = (collected_data_t const *)packet;

	unsigned int length = msg->length;

	void const * msgdata = (msg + 1); // Get the pointer after the message

	PEDPRINTF("PEGP: Adding %u pieces of data in round %u\n", length, msg->round_count);

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		void const * item = CNODE_DATA_INDEX(msgdata, i, pegp->data_size);

		void * stored = map_get(&pegp->received_data, item);

		if (stored == NULL)
		{
			stored = malloc(pegp->data_size);
			map_put(&pegp->received_data, stored);
		}

		memcpy(stored, item, pegp->data_size);
	}
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	pegp_conn_t * pegp = conncvt_tree_agg(conn);

	PEDPRINTF("PEGP: Setup finsihed\n");

	if (tree_agg_is_leaf(conn))
	{
		leds_on(LEDS_RED);
	}

	process_start(&send_data_process, (void *)pegp);
}

static void tree_aggregate_update(tree_agg_conn_t * tconn, void * voiddata, void const * to_apply, unsigned int to_apply_length)
{
	pegp_conn_t * pegp = conncvt_tree_agg(tconn);

	PEDPRINTF("PEGP: Update local data\n");

	toggle_led_for(LEDS_RED, CLOCK_SECOND);

	unique_array_t * data = &((aggregation_data_t *)voiddata)->list;
	collected_data_t const * data_to_apply = (collected_data_t const *)to_apply;

	void const * msgdata = (data_to_apply + 1); //get the pointer after the message

	unsigned int i;
	for (i = 0; i < data_to_apply->length; ++i)
	{
		void const * item = CNODE_DATA_INDEX(msgdata, i, pegp->data_size);

		if (!unique_array_contains(data, item))
		{
			void * tmp = malloc(pegp->data_size);
			memcpy(tmp, item, pegp->data_size);
			unique_array_append(data, tmp);
		}
	}
}

// Add our own one hop data to the list
static void tree_aggregate_own(tree_agg_conn_t * tconn, void * ptr)
{
	pegp_conn_t * pegp = conncvt_tree_agg(tconn);

	PEDPRINTF("PEGP: Update local data with own data\n");

	unique_array_t * data = &((aggregation_data_t *)ptr)->list;

	// Allocate and fill in our data
	void * msg = malloc(pegp->data_size);
	pegp->data_fn(msg);

	unique_array_append(data, msg);
}

// Store an inbound packet to the datastructure
// Arguments are: Connection, Packet, packet length
static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	pegp_conn_t * pegp = conncvt_tree_agg(conn);

	PEDPRINTF("PEGP: Store len=%u\n", length);

	collected_data_t const * msg = (collected_data_t const *)packet;

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;

	conn_data->round_count = msg->round_count;

	unique_array_init(&conn_data->list, &rimeaddr_equality, &free);
	
	// Store the received data
	tree_aggregate_update(conn, conn_data, packet, length);
}

// Write the data structure to the outbout packet buffer
static void tree_agg_write_data_to_packet(tree_agg_conn_t * conn, void ** data, size_t * packet_length)
{
	pegp_conn_t * pegp = conncvt_tree_agg(conn);

	PEDPRINTF("PEGP: Writing data to packet\n");

	// Take all data, write a struct to the buffer at the start, 
	// containing the length of the packet (as the number of node_data_t)
	// write the each one to memory
	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;
	unsigned int length = unique_array_length(&conn_data->list);
	
	*packet_length = sizeof(collected_data_t) + (pegp->data_size * length);
	*data = malloc(*packet_length);

	collected_data_t * msg = (collected_data_t *)*data;
	msg->length = length;
	msg->round_count = conn_data->round_count;

	PEDPRINTF("PEGP: Write len=%d dlen=%d\n", msg->length, *packet_length);

	// Get the pointer after the message
	void * msgdata = (msg + 1);

	unsigned int i = 0;
	unique_array_elem_t elem;
	for (elem = unique_array_first(&conn_data->list); 
		unique_array_continue(&conn_data->list, elem);
		elem = unique_array_next(elem))
	{
		void * original = unique_array_data(&conn_data->list, elem);
		memcpy(NODE_DATA_INDEX(msgdata, i, pegp->data_size), original, pegp->data_size);

		++i;
	}

	// Free the data here
	unique_array_free(&conn_data->list);
}


static void pm_predicate_failed(predicate_manager_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	pegp_conn_t * pegp = conncvt_predicate_manager(conn);

	pegp->predicate_failed(pegp, from, hops);
}

static const predicate_manager_callbacks_t pm_callbacks = { NULL, &pm_predicate_failed };

static const tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};

static const neighbour_agg_callbacks_t neighbour_callbacks = {&handle_neighbour_data};


PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;
	static uint8_t round_count;
	static pegp_conn_t * pegp;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	pegp = (pegp_conn_t *)data;
	
	round_count = 0;

	while (true)
	{
		etimer_set(&et, ROUND_LENGTH);

		// Leaf nodes start tree aggregation
		if (tree_agg_is_leaf(&pegp->aggconn))
		{
			// We should be set up by now
			// Start sending data up the tree

			size_t data_length = sizeof(collected_data_t) + pegp->data_size;
			collected_data_t * msg = (collected_data_t *)malloc(data_length);

			msg->round_count = round_count;
			msg->length = 1;

			// Get the pointer after the message that will contain the nodes data
			void * msgdata = (msg + 1);
			pegp->data_fn(msgdata);

			tree_agg_send(&pegp->aggconn, msg, data_length);

			free(msg);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		++round_count;
	}

exit:
	(void)0;
	PROCESS_END();
}

static pegp_conn_t * global_pegp_conn;

static void pretend_node_data(void * data)
{
	if (data != NULL)
	{
		void * stored_data = map_get(&global_pegp_conn->received_data, &global_pegp_conn->pred_simulated_node);

		memcpy(data, stored_data, global_pegp_conn->data_size);
	}
}

static void data_evaluation(pegp_conn_t * pegp)
{
	PEDPRINTF("PEGP: Start Eval\n");

	map_t const * predicate_map = predicate_manager_get_map(&pegp->predconn);

	map_elem_t elem;
	for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pred = (predicate_detail_entry_t const *)map_data(predicate_map, elem);
		
		// Copy in the simulated node
	    rimeaddr_copy(&pegp->pred_simulated_node, &pred->target);

	    // Get the maximum number of hops needed for this predcate
	    const uint8_t max_hops = predicate_manager_max_hop(pred);

		hop_manager_init(&pegp->hop_data);

		// Array of nodes that have been seen and checked so far
	    unique_array_t seen_nodes;
	    unique_array_init(&seen_nodes, &rimeaddr_equality, &free);

	    // Start with the destination node
	    unique_array_append(&seen_nodes, rimeaddr_clone(&pred->target)); 

		// Array of nodes that we need the neighbours for
	    unique_array_t target_nodes;
	    unique_array_init(&target_nodes, &rimeaddr_equality, &free);

	    // Start with the destination node
	    unique_array_append(&target_nodes, rimeaddr_clone(&pred->target));

	    // Array of nodes, we gathered this round
	    unique_array_t acquired_nodes;
	    unique_array_init(&acquired_nodes, &rimeaddr_equality, &free);

	    // Get the data for each hop level
		uint8_t hops;
		for (hops = 1; hops <= max_hops; ++hops)
		{
		    // For each node in the target nodes, get the immediate neighbours,
			unique_array_elem_t target;
			for (target = unique_array_first(&target_nodes); 
				unique_array_continue(&target_nodes, target); 
				target = unique_array_next(target))
			{
				rimeaddr_t * t = (rimeaddr_t *)unique_array_data(&target_nodes, target); 
				PEDPRINTF("PEGP: Checking:%s %d hops\n", addr2str(t), hops);

				// Go through the neighbours for the node
				unique_array_elem_t neighbours_elem;
				for (neighbours_elem = unique_array_first(&pegp->neighbour_info); 
					unique_array_continue(&pegp->neighbour_info, neighbours_elem); 
					neighbours_elem = unique_array_next(neighbours_elem))
				{
					// The neighbour found
					rimeaddr_pair_t * neighbours = unique_array_data(&pegp->neighbour_info, neighbours_elem);

					rimeaddr_t * neighbour = NULL;

					if (rimeaddr_cmp(&neighbours->first, t))
					{
						neighbour = &neighbours->second;
					}

					if (rimeaddr_cmp(&neighbours->second, t))
					{
						neighbour = &neighbours->first;
					}

					if (neighbour != NULL)
					{
						PEDPRINTF("PEGP: Eval: Checking neighbour %s\n", addr2str(neighbour));

						// If the neighbour hasn't been seen before
						if (!unique_array_contains(&seen_nodes, neighbour)) 
						{
							void * nd = map_get(&pegp->received_data, neighbour);

							if (nd == NULL)
							{
								PEDPRINTF("PEGP: ERROR no info on %s\n", addr2str(neighbour));
							}
							else
							{
								// Add the node to the target nodes for the next round
								unique_array_append(&acquired_nodes, rimeaddr_clone(neighbour));

								hop_manager_record(&pegp->hop_data, hops, nd, pegp->data_size);
							}
						}
					}
				}
			}

			// Been through targets add them to the seen nodes
			// This call will steal the memory from target_nodes and leave it empty
			unique_array_merge(&seen_nodes, &target_nodes, NULL);

			// Add in the acquired nodes
			unique_array_merge(&target_nodes, &acquired_nodes, &rimeaddr_clone);
		}

		// Generate array of all the data
		void * all_neighbour_data = NULL;

		// Number of nodes we pass to the evaluation
		const unsigned int max_size = hop_manager_max_size(&pegp->hop_data);

		if (max_size > 0)
		{
			all_neighbour_data = malloc(pegp->data_size * max_size);

			// Position in all_neighbour_data
			unsigned int count = 0;

			uint8_t i;
			for (i = 1; i <= max_hops; ++i)
			{
				map_t * hop_map = hop_manager_get(&pegp->hop_data, i);

				map_elem_t aelem;
				for (aelem = map_first(hop_map); map_continue(hop_map, aelem); aelem = map_next(aelem))
				{
					void * mapdata = map_data(hop_map, aelem);
					memcpy(NODE_DATA_INDEX(all_neighbour_data, count, pegp->data_size), mapdata, pegp->data_size);
					++count;
				}

				PEDPRINTF("PEGP: Eval: i=%u Count=%d/%d len=%d\n", i, count, max_size, map_length(hop_map));
			}
		}

		// Need to set the global conn, so that pretend_node_data
		// has access to it
		global_pegp_conn = pegp;

		bool evaluation_result = evaluate_predicate(&pegp->predconn,
			pretend_node_data, pegp->data_size,
			pegp->function_details, pegp->functions_count,
			&pegp->hop_data,
			all_neighbour_data, max_size, pred);

#if 0
		if (evaluation_result)
		{
			PEDPRINTF("PEGP: TRUE\n");
		}
		else
		{
			PEDPRINTF("PEGP: FAILED (%s)\n", error_message());
		}
#endif
		
		//predicate_manager_send_response(&predconn, &hop_data,
		//	pe, all_neighbour_data, sizeof(node_data_t), max_size);

		free(all_neighbour_data);

		hop_manager_reset(&pegp->hop_data);
		unique_array_free(&target_nodes);
		unique_array_free(&seen_nodes);
		unique_array_free(&acquired_nodes);
	}

	// Empty details received and let the next round fill them up
	map_clear(&pegp->received_data);
	unique_array_clear(&pegp->neighbour_info);

	PEDPRINTF("PEGP: Round=%u\n", pegp->pred_round_count);
	
	pegp->pred_round_count += 1;
}

PROCESS_THREAD(data_evaluation_process, ev, data)
{
	static struct etimer et;
	static pegp_conn_t * pegp;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	pegp = (pegp_conn_t *) data;

	map_init(&pegp->received_data, &rimeaddr_equality, &free);

	while (true)
	{
		etimer_set(&et, pegp->predicate_period);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		data_evaluation(pegp);
	}

exit:
	map_free(&pegp->received_data);

	PROCESS_END();
}


void pegp_start_delayed2(pegp_conn_t * conn)
{
	PEDPRINTF("PEGP: Starting Data Aggregation\n");

	tree_agg_open(&conn->aggconn, conn->sink, 140, 170, sizeof(aggregation_data_t), &callbacks);

	// If sink start the evaluation process to run in the background
	if (rimeaddr_cmp(&rimeaddr_node_addr, conn->sink))
	{
		process_start(&data_evaluation_process, (void *)conn);
	}
}

void pegp_start_delayed1(pegp_conn_t * conn)
{
	//printf("PEGP: Starting Neighbour Aggregation\n");

	neighbour_aggregate_open(&conn->nconn, conn->sink, 121, 110, 150, &neighbour_callbacks);

	ctimer_set(&conn->ct_startup, 120 * CLOCK_SECOND, &pegp_start_delayed2, conn);
}


bool pegp_start(pegp_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pegp_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count,
	clock_time_t predicate_period)
{
	if (conn == NULL || predicate_failed == NULL || data_fn == NULL ||
		sink == NULL || data_size == 0)
	{
		return false;
	}

	conn->sink = sink;
	conn->data_fn = data_fn;
	conn->data_size = data_size;
	conn->predicate_failed = predicate_failed;
	conn->pred_round_count = 0;

	conn->function_details = function_details;
	conn->functions_count = functions_count;

	conn->predicate_period = predicate_period;


	predicate_manager_open(&conn->predconn, 135, 129, sink, TRICKLE_INTERVAL, &pm_callbacks);

	if (rimeaddr_cmp(&rimeaddr_node_addr, sink))
	{
		PEDPRINTF("PEGP: We are sink node.\n");

		predicate_manager_start_serial_input(&conn->predconn);
	}

	// Setup the map
	unique_array_init(&conn->neighbour_info, &rimeaddr_pair_equality, &free);

	// Wait for some time to let process start up and perform neighbour detect
	ctimer_set(&conn->ct_startup, 10 * CLOCK_SECOND, &pegp_start_delayed1, conn);

	return true;
}

void pegp_stop(pegp_conn_t * conn)
{
	if (conn != NULL)
	{
		process_exit(&data_evaluation_process);
		process_exit(&send_data_process);

		ctimer_stop(&conn->ct_startup);

		tree_agg_close(&conn->aggconn);
		neighbour_aggregate_close(&conn->nconn);
		unique_array_free(&conn->neighbour_info);
		predicate_manager_close(&conn->predconn);
	}
}



#ifdef PEGP_APPLICATION

// Struct for the list of node_data. It contains owner_addr, temperature and humidity. 
typedef struct
{
	rimeaddr_t addr;
	nfloat temp;
	nint humidity;
	//nint light1;
	//nint light2;
} node_data_t;

///
/// Start VM Helper Functions
///
static void const * get_addr(void const * ptr)
{
	return &((node_data_t const *)ptr)->addr;
}

static void const * get_temp(void const * ptr)
{
	return &((node_data_t const *)ptr)->temp;
}

static void const * get_humidity(void const * ptr)
{
	return &((node_data_t const *)ptr)->humidity;
}

static const function_details_t func_det[] =
{
	{ 0, TYPE_INTEGER, &get_addr },
	{ 2, TYPE_FLOATING, &get_temp },
	{ 3, TYPE_INTEGER, &get_humidity },
};

static void node_data(void * data)
{
	if (data != NULL)
	{
		node_data_t * nd = (node_data_t *)data;

		// Store the current nodes address
		rimeaddr_copy(&nd->addr, &rimeaddr_node_addr);

		SENSORS_ACTIVATE(sht11_sensor);
		int raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		int raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		nd->temp = sht11_temperature(raw_temperature);
		nd->humidity = sht11_relative_humidity_compensated(raw_humidity, nd->temp);

		/*SENSORS_ACTIVATE(light_sensor);
		int raw_light1 = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
		int raw_light2 = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);
		SENSORS_DEACTIVATE(light_sensor);

		nd->light1 = (nint)s1087_light1(raw_light1);
		nd->light2 = (nint)s1087_light1(raw_light2);*/
	}
}

static bool send_example_predicate(pegp_conn_t * pegp, rimeaddr_t const * destination, uint8_t id)
{
	if (pegp == NULL || destination == NULL)
		return false;

	static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};
	
	static var_elem_t const var_details[2] = {
		{2, 255}, {1, 254}
	};

	static const uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
	static const uint8_t var_details_length = sizeof(var_details)/sizeof(var_details[0]);

	return predicate_manager_create(&pegp->predconn,
		id, destination,
		program_bytecode, bytecode_length,
		var_details, var_details_length);
}

static void predicate_failed(pegp_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("PEGP: Response from %s, %u, %u hops. Failed pred %u.\n",
		addr2str(from), packetbuf_datalen(), hops, response->predicate_id);
}

PROCESS(mainProcess, "Main Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static rimeaddr_t sink;
	static pegp_conn_t pegp;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	PEDPRINTF("PEGP: Process Started.\n");

	// Init code
#ifdef NODE_ID
	node_id_burn(NODE_ID);
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL);
#endif

	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	// Set the address of the base station
	sink.u8[0] = 1;
	sink.u8[1] = 0;

	pegp_start(&pegp,
		&sink, &node_data, sizeof(node_data_t), &predicate_failed,
		func_det, sizeof(func_det)/sizeof(func_det[0]),
		4 * 60 * CLOCK_SECOND);

	if (rimeaddr_cmp(&sink, &rimeaddr_node_addr))
	{
		rimeaddr_t destination;
		destination.u8[0] = 5;
		destination.u8[1] = 0;

		send_example_predicate(&pegp, &destination, 0);
	}

	// This is where the application would be
	while (true)
	{
		// Wait for other nodes to initialize.
		etimer_set(&et, 20 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

exit:
	pegp_stop(&pegp);

	PROCESS_END();
}

#endif
