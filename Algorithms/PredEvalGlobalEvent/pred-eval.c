#include "pege.h"

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


#ifdef PE_DEBUG
#	define PEDPRINTF(...) printf(__VA_ARGS__)
#else
#	define PEDPRINTF(...)
#endif

#define ROUND_LENGTH (clock_time_t)(5 * 60 * CLOCK_SECOND)
#define INITIAL_ROUND_LENGTH (clock_time_t)(7 * 60 * CLOCK_SECOND)
#define TRICKLE_INTERVAL (clock_time_t)(2 * CLOCK_SECOND)

#define NODE_DATA_INDEX(array, index, size) \
	(((char *)array) + ((index) * (size)))

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


static inline pege_conn_t * conncvt_tree_agg(tree_agg_conn_t * conn)
{
	return (pege_conn_t *)conn;
}

static inline pege_conn_t * conncvt_neighbour_agg(neighbour_agg_conn_t * conn)
{
	return (pege_conn_t *)
		(((char *)conn) - sizeof(tree_agg_conn_t));
}

static inline pege_conn_t * conncvt_predicate_manager(predicate_manager_conn_t * conn)
{
	return (pege_conn_t *)
		(((char *)conn) - sizeof(tree_agg_conn_t) - sizeof(neighbour_agg_conn_t));
}


PROCESS(data_evaluation_process, "Data eval");
PROCESS(send_data_process, "Send data process");


static void handle_neighbour_data(neighbour_agg_conn_t * conn, rimeaddr_pair_t const * pairs,
	unsigned int length, unsigned int round_count)
{
	pege_conn_t * pege = conncvt_neighbour_agg(conn);

	PEDPRINTF("PEGE: Handling neighbour data round=%u length=%u\n", round_count, length);

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		if (!unique_array_contains(&pege->neighbour_info, &pairs[i]))
		{
			unique_array_append(&pege->neighbour_info, rimeaddr_pair_clone(&pairs[i]));
		}
	}
}

// Sink recieved final set of data
static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source, void const * packet, unsigned int packet_length)
{
	pege_conn_t * pege = conncvt_tree_agg(conn);

	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	// Extract data from packet buffer
	collected_data_t const * msg = (collected_data_t const *)packet;

	unsigned int length = msg->length;

	void const * msgdata = (msg + 1); // Get the pointer after the message

	PEDPRINTF("PEGE: Adding %u pieces of data in round %u\n", length, msg->round_count);

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		void const * item = ((char const *)msgdata) + (i * pege->data_size);

		void * stored = map_get(&pege->received_data, item);

		if (stored == NULL)
		{
			stored = malloc(pege->data_size);
			map_put(&pege->received_data, stored);
		}

		memcpy(stored, item, pege->data_size);
	}
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	pege_conn_t * pege = conncvt_tree_agg(conn);

	PEDPRINTF("PEGE: Setup finsihed\n");

	if (tree_agg_is_leaf(conn))
	{
		PEDPRINTF("PEGE: Is leaf starting data aggregation\n");

		leds_on(LEDS_RED);
	}

	process_start(&send_data_process, (void *)pege);
}

static void tree_aggregate_update(tree_agg_conn_t * tconn, void * voiddata, void const * to_apply, unsigned int to_apply_length)
{
	pege_conn_t * pege = conncvt_tree_agg(tconn);

	PEDPRINTF("PEGE: Update local data\n");

	toggle_led_for(LEDS_RED, CLOCK_SECOND);

	unique_array_t * data = &((aggregation_data_t *)voiddata)->list;
	collected_data_t const * data_to_apply = (collected_data_t const *)to_apply;

	void const * msgdata = (data_to_apply + 1);

	unsigned int i;
	for (i = 0; i < data_to_apply->length; ++i)
	{
		void const * item = ((char const *)msgdata) + (i * pege->data_size);

		if (!unique_array_contains(data, item))
		{
			void * tmp = malloc(pege->data_size);
			memcpy(tmp, item, pege->data_size);
			unique_array_append(data, tmp);
		}
	}
}

// Add our own one hop data to the list
static void tree_aggregate_own(tree_agg_conn_t * tconn, void * ptr)
{
	pege_conn_t * pege = conncvt_tree_agg(tconn);

	PEDPRINTF("PEGE: Update local data with own data\n");

	unique_array_t * data = &((aggregation_data_t *)ptr)->list;

	// Allocate and fill in our data
	void * msg = malloc(pege->data_size);
	pege->data_fn(msg);

	unique_array_append(data, msg);
}

// Store an inbound packet to the datastructure
// Arguments are: Connection, Packet, packet length
static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	pege_conn_t * pege = conncvt_tree_agg(conn);

	PEDPRINTF("PEGE: Store Packet length=%u\n", length);

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
	pege_conn_t * pege = conncvt_tree_agg(conn);

	// Take all data, write a struct to the buffer at the start, 
	// containing the length of the packet (as the number of node_data_t)
	// write the each one to memory
	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;
	unsigned int length = unique_array_length(&conn_data->list);
	
	*packet_length = sizeof(collected_data_t) + (pege->data_size * length);
	*data = malloc(*packet_length);

	collected_data_t * msg = (collected_data_t *)*data;
	msg->length = length;
	msg->round_count = conn_data->round_count;

	printf("PEGE: Writing len=%d dlen=%d\n", msg->length, *packet_length);

	// Get the pointer after the message
	void * msgdata = (msg + 1);

	unsigned int i = 0;
	unique_array_elem_t elem;
	for (elem = unique_array_first(&conn_data->list); 
		unique_array_continue(&conn_data->list, elem);
		elem = unique_array_next(elem))
	{
		void * original = unique_array_data(&conn_data->list, elem);
		memcpy(NODE_DATA_INDEX(msgdata, i, pege->data_size), original, pege->data_size);

		++i;
	}

	// Free the data here
	unique_array_free(&conn_data->list);
}


static void pm_predicate_failed(predicate_manager_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	pege_conn_t * pege = conncvt_predicate_manager(conn);

	pege->predicate_failed(pege, from, hops);
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
	static void * current_data;
	static void * previous_data;
	static pege_conn_t * pege;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	pege = (pege_conn_t *)data;

	current_data = malloc(pege->data_size);
	previous_data = malloc(pege->data_size);
	
	round_count = 0;

	while (true)
	{
		etimer_set(&et, ROUND_LENGTH);

		// Find the current data
		pege->data_fn(current_data);

		// Usually we would have leaf nodes starting sending data back up the tree
		// instead any node may do so, but only if its data has changed.
		// However, for the first round, we only let the leaves do the sending to save on
		// energy
		if (
			(round_count == 0 && tree_agg_is_leaf(&pege->aggconn)) ||
			(round_count > 0 && pege->differs_fn(current_data, previous_data))
		   )
		{
			// We should be set up by now
			// Start sending data up the tree

			size_t data_length = sizeof(collected_data_t) + pege->data_size;
			collected_data_t * msg = (collected_data_t *)malloc(data_length);

			msg->round_count = round_count;
			msg->length = 1;

			// Get the pointer after the message that will contain the nodes data and fill it
			void * msgdata = (msg + 1);
			memcpy(msgdata, current_data, pege->data_size);

			// Remember the changed data
			memcpy(previous_data, current_data, pege->data_size);

			tree_agg_send(&pege->aggconn, msg, data_length);

			free(msg);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		++round_count;
	}

exit:
	free(current_data);
	free(previous_data);

	PROCESS_END();
}

static pege_conn_t * global_pege_conn;

// This function would typically just return the current nodes data
// however because we are evaluating a predicate from a different node
// we need to return that node's data
static void pretend_node_data(void * data)
{
	if (data != NULL)
	{
		void * stored_data = map_get(&global_pege_conn->received_data, &global_pege_conn->pred_simulated_node);

		memcpy(data, stored_data, global_pege_conn->data_size);
	}
}

static void data_evaluation(pege_conn_t * pege)
{
	PEDPRINTF("PEGE: Eval: Beginning Evaluation\n");

	map_t const * predicate_map = predicate_manager_get_map(&pege->predconn);

	map_elem_t elem;
	for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pred = (predicate_detail_entry_t const *)map_data(predicate_map, elem);
		
		// Copy in the simulated node
	    rimeaddr_copy(&pege->pred_simulated_node, &pred->target);

	    // Get the maximum number of hops needed for this predcate
	    const uint8_t max_hops = predicate_manager_max_hop(pred);

	    hop_data_t hop_data;
		hop_manager_init(&hop_data);

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
				printf("PEGE: Eval: Checking=%s %d hops\n", addr2str(t), hops);

				// Go through the neighbours for the node
				unique_array_elem_t neighbours_elem;
				for (neighbours_elem = unique_array_first(&pege->neighbour_info); 
					unique_array_continue(&pege->neighbour_info, neighbours_elem); 
					neighbours_elem = unique_array_next(neighbours_elem))
				{
					// The neighbour found
					rimeaddr_pair_t * neighbours = unique_array_data(&pege->neighbour_info, neighbours_elem);

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
						PEDPRINTF("PEGE: Eval: Checking neighbour %s\n", addr2str(neighbour));

						// If the neighbour hasn't been seen before
						if (!unique_array_contains(&seen_nodes, neighbour)) 
						{
							void * nd = map_get(&pege->received_data, neighbour);

							if (nd == NULL)
							{
								printf("PEGE: ERROR: no info on %s\n", addr2str(neighbour));
							}
							else
							{
								// Add the node to the target nodes for the next round
								unique_array_append(&acquired_nodes, rimeaddr_clone(neighbour));

								hop_manager_record(&hop_data, hops, nd, pege->data_size);
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
		const unsigned int max_size = hop_manager_max_size(&hop_data);

		if (max_size > 0)
		{
			all_neighbour_data = malloc(pege->data_size * max_size);

			// Position in all_neighbour_data
			unsigned int count = 0;

			uint8_t i;
			for (i = 1; i <= max_hops; ++i)
			{
				map_t * hop_map = hop_manager_get(&hop_data, i);

				array_list_elem_t aelem;
				for (aelem = map_first(hop_map); map_continue(hop_map, aelem); aelem = map_next(aelem))
				{
					void * mapdata = map_data(hop_map, aelem);
					memcpy(NODE_DATA_INDEX(all_neighbour_data, count, pege->data_size), mapdata, pege->data_size);
					++count;
				}

				printf("PEGE: Eval: i=%d Count=%d/%d len=%d\n", i, count, max_size, map_length(hop_map));
			}
		}

		// We need to set the global data needed for pretend_node_data
		global_pege_conn = pege;

		bool evaluation_result = evaluate_predicate(
			pretend_node_data, pege->data_size,
			pege->function_details, pege->functions_count,
			&hop_data,
			pred->bytecode, pred->bytecode_length,
			all_neighbour_data,
			pred->variables_details, pred->variables_details_length);

		if (evaluation_result)
		{
			printf("PEGE: TRUE\n");
		}
		else
		{
			printf("PEGE: FAILED (%s)\n", error_message());
		}

		predicate_manager_send_response(&pege->predconn, &hop_data,
			pred, all_neighbour_data, pege->data_size, max_size);

		free(all_neighbour_data);

		hop_manager_reset(&hop_data);
		unique_array_free(&target_nodes);
		unique_array_free(&seen_nodes);
		unique_array_free(&acquired_nodes);
	}

	// Empty details received and let the next round fill them up
	// We do not clear received_data, as that is sent only when it changes
	unique_array_clear(&pege->neighbour_info);

	printf("PEGE: Finishing=%u received_data=%u\n",
		pege->pred_round_count, map_length(&pege->received_data));
	
	pege->pred_round_count += 1;
}

PROCESS_THREAD(data_evaluation_process, ev, data)
{
	static struct etimer et;
	static pege_conn_t * pege;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	pege = (pege_conn_t *) data;

	map_init(&pege->received_data, &rimeaddr_equality, &free);

	while (true)
	{
		etimer_set(&et, INITIAL_ROUND_LENGTH);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		data_evaluation(pege);
	}

exit:
	map_free(&pege->received_data);

	PROCESS_END();
}


void pege_start_delayed2(pege_conn_t * conn)
{
	PEDPRINTF("PEGE: Starting Data Aggregation\n");

	tree_agg_open(&conn->aggconn, conn->sink, 140, 170, sizeof(aggregation_data_t), &callbacks);

	// If sink start the evaluation process to run in the background
	if (rimeaddr_cmp(&rimeaddr_node_addr, conn->sink))
	{
		process_start(&data_evaluation_process, (void *)conn);
	}
}

void pege_start_delayed1(pege_conn_t * conn)
{
	//printf("PEGE: Starting Neighbour Aggregation\n");

	neighbour_aggregate_open(&conn->nconn, conn->sink, 121, 110, 150, &neighbour_callbacks);

	ctimer_set(&conn->ct_startup, 120 * CLOCK_SECOND, &pege_start_delayed2, conn);
}


bool pege_start(pege_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pege_data_differs_fn differs_fn, pege_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count)
{
	if (conn == NULL || predicate_failed == NULL || data_fn == NULL ||
		sink == NULL || data_size == 0 || differs_fn == NULL)
	{
		return false;
	}

	conn->sink = sink;
	conn->data_fn = data_fn;
	conn->data_size = data_size;
	conn->differs_fn = differs_fn;
	conn->predicate_failed = predicate_failed;
	conn->pred_round_count = 0;

	conn->function_details = function_details;
	conn->functions_count = functions_count;


	predicate_manager_open(&conn->predconn, 135, 129, sink, TRICKLE_INTERVAL, &pm_callbacks);

	if (rimeaddr_cmp(&rimeaddr_node_addr, sink))
	{
		PEDPRINTF("PEGE: We are sink node.\n");

		predicate_manager_start_serial_input(&conn->predconn);
	}

	// Setup the map
	unique_array_init(&conn->neighbour_info, &rimeaddr_pair_equality, &free);

	// Wait for some time to let process start up and perform neighbour detect
	ctimer_set(&conn->ct_startup, 10 * CLOCK_SECOND, &pege_start_delayed1, conn);

	return true;
}

void pege_stop(pege_conn_t * conn)
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



#ifdef PEGE_APPLICATION

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

// Our differs function
// This is simply an arbitrary comparison that sees if data
// has significantly changed. What is significant is up to the
// application to decide.
static bool node_data_differs(void const * data1, void const * data2)
{
	node_data_t const * nd1 = (node_data_t const *)data1;
	node_data_t const * nd2 = (node_data_t const *)data2;

	if (nd1 == NULL && nd2 == NULL)
	{
		return false;
	}
	else if (nd1 == NULL || nd2 == NULL)
	{
		return true;
	}
	else
	{
		double temp_diff = nd1->temp - nd2->temp;
		if (temp_diff < 0) temp_diff = -temp_diff;

		int humidity_diff = nd1->humidity - nd2->humidity;
		if (humidity_diff < 0) humidity_diff = -humidity_diff;

		return temp_diff > 3 || humidity_diff > 5;
	}
}


static bool send_example_predicate(pege_conn_t * pege, rimeaddr_t const * destination, uint8_t id)
{
	if (pege == NULL || destination == NULL)
		return false;

	static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};
	
	static var_elem_t var_details[2];
	var_details[0].hops = 2;
	var_details[0].var_id = 255;
	var_details[1].hops = 1;
	var_details[1].var_id = 254;

	uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
	uint8_t var_details_length = 2;

	return predicate_manager_create(&pege->predconn,
		id, destination,
		program_bytecode, bytecode_length,
		var_details, var_details_length);
}

static void predicate_failed(pege_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("PEGE: Response from %s, %u, %u hops away. Failed predicate %u.\n",
		addr2str(from), packetbuf_datalen(), hops, response->predicate_id);
}

PROCESS(mainProcess, "Main Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static rimeaddr_t sink;
	static pege_conn_t pege;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	PEDPRINTF("PEGE: Process Started.\n");

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

	pege_start(&pege,
		&sink, &node_data, sizeof(node_data_t), &node_data_differs, &predicate_failed,
		func_det, sizeof(func_det)/sizeof(func_det[0]));

	if (rimeaddr_cmp(&sink, &rimeaddr_node_addr))
	{
		rimeaddr_t destination;
		destination.u8[0] = 5;
		destination.u8[1] = 0;

		send_example_predicate(&pege, &destination, 0);
	}

	// This is where the application would be
	while (true)
	{
		// Wait for other nodes to initialize.
		etimer_set(&et, 20 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

exit:
	pege_stop(&pege);

	PROCESS_END();
}

#endif
