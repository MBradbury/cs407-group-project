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

#include "net/tree-aggregator.h"
#include "neighbour-aggregate.h"

#include "predlang.h"

#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "containers/array-list.h"
#include "containers/unique-array.h"
#include "containers/map.h"

#include "predicate-manager.h"
#include "hop-data-manager.h"

#ifdef PE_DEBUG
#	define PEDPRINTF(...) printf(__VA_ARGS__)
#else
#	define PEDPRINTF(...)
#endif

static void data_evaluation(void);

#define ROUND_LENGTH (clock_time_t)(5 * 60 * CLOCK_SECOND)
#define INITIAL_ROUND_LENGTH (clock_time_t)(7 * 60 * CLOCK_SECOND)
#define TRICKLE_INTERVAL (clock_time_t)(2 * CLOCK_SECOND)

// Map containing rimeaddr_pair_t
static unique_array_t neighbour_info;

// Map containing node_data_t
static map_t received_data;

// Used for simulating evaluating a predicate on a node
static rimeaddr_t pred_simulated_node;

static unsigned int pred_round_count;

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

// Struct for the list of node_data. It contains owner_addr, round count, temperature and humidity. 
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

static void our_node_data(void * data)
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

// This function would typically just return the current nodes data
// however because we are evaluating a predicate from a different node
// we need to return that node's data
static void node_data(void * data)
{
	if (data != NULL)
	{
		node_data_t * stored_data = (node_data_t *)map_get(&received_data, &pred_simulated_node);

		memcpy(data, stored_data, sizeof(node_data_t));
	}
}
///
/// End VM Helper Functions
///


//TODO: add clearing of previous neighbour information
/* to be called when neighbour aggregate gets some data to add */
static void handle_neighbour_data(rimeaddr_pair_t const * pairs, unsigned int length, unsigned int round_count)
{
	PEDPRINTF("PE GE: Handling neighbour data round=%u length=%u\n", round_count, length);

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		if (!unique_array_contains(&neighbour_info, &pairs[i]))
		{
			rimeaddr_pair_t * p = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
			*p = pairs[i];
			unique_array_append(&neighbour_info, p);
		}
	}
}

PROCESS(data_gather, "Data Gather");
PROCESS(send_data_process, "Send data process");

AUTOSTART_PROCESSES(&data_gather);

// Sink recieved final set of data
static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source, void const * packet, unsigned int packet_length)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	// Extract data from packet buffer
	collected_data_t const * msg = (collected_data_t const *)packet;

	unsigned int length = msg->length;

	node_data_t const * msgdata = (node_data_t const *)(msg + 1); // Get the pointer after the message

	printf("PE GE: Adding %u pieces of data in round %u\n", length, msg->round_count);

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		printf("PE GE: Data Node: %s Temp:%d, Humidity: %d\n",
			addr2str(&msgdata[i].addr),
			(int)msgdata[i].temp,
			msgdata[i].humidity);

		void * stored = map_get(&received_data, &msgdata[i]);

		if (stored == NULL)
		{
			stored = malloc(sizeof(node_data_t));
			map_put(&received_data, stored);
		}

		memcpy(stored, &msgdata[i], sizeof(node_data_t));
	}
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	PEDPRINTF("PE GE: Setup finsihed\n");

	if (tree_agg_is_leaf(conn))
	{
		PEDPRINTF("PE GE: Is leaf starting data aggregation\n");

		leds_on(LEDS_RED);

		process_start(&send_data_process, NULL);
	}
}

static void tree_aggregate_update(tree_agg_conn_t * tconn, void * voiddata, void const * to_apply, unsigned int to_apply_length)
{
	PEDPRINTF("PE GE: Update local data\n");

	toggle_led_for(LEDS_RED, CLOCK_SECOND);

	unique_array_t * data = &((aggregation_data_t *)voiddata)->list;
	collected_data_t const * data_to_apply = (collected_data_t const *)to_apply;

	node_data_t const * msgdata = (node_data_t const *)(data_to_apply + 1); //get the pointer after the message

	unsigned int i;
	for (i = 0; i < data_to_apply->length; ++i)
	{
		if (!unique_array_contains(data, &msgdata[i]))
		{
			void * tmp = malloc(sizeof(node_data_t));
			memcpy(tmp, &msgdata[i], sizeof(node_data_t));
			unique_array_append(data, tmp);
		}
	}
}

// Add our own one hop data to the list
static void tree_aggregate_own(tree_agg_conn_t * tconn, void * ptr)
{
	PEDPRINTF("PE GE: Update local data with own data\n");

	unique_array_t * data = &((aggregation_data_t *)ptr)->list;

	// Allocate and fill in our data
	node_data_t * msg = (node_data_t *)malloc(sizeof(node_data_t));
	our_node_data(msg);

	unique_array_append(data, msg);
}

// Store an inbound packet to the datastructure
// Arguments are: Connection, Packet, packet length
static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	PEDPRINTF("PE GE: Store Packet length=%u\n", length);

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
	// Take all data, write a struct to the buffer at the start, 
	// containing the length of the packet (as the number of node_data_t)
	// write the each one to memory
	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;
	unsigned int length = unique_array_length(&conn_data->list);
	
	*packet_length = sizeof(collected_data_t) + sizeof(node_data_t) * length;
	*data = malloc(*packet_length);

	collected_data_t * msg = (collected_data_t *)*data;
	msg->length = length;
	msg->round_count = conn_data->round_count;

	printf("PE GE: Writing packet, length:%d data length:%d\n", msg->length, *packet_length);

	// Get the pointer after the message
	node_data_t * msgdata = (node_data_t *)(msg + 1);

	unsigned int i = 0;
	unique_array_elem_t elem;
	for (elem = unique_array_first(&conn_data->list); 
		unique_array_continue(&conn_data->list, elem);
		elem = unique_array_next(elem))
	{
		node_data_t * original = (node_data_t *)unique_array_data(&conn_data->list, elem);
		memcpy(&msgdata[i], original, sizeof(node_data_t));

		++i;
	}

	// Free the data here
	unique_array_free(&conn_data->list);
}


static void pm_predicate_failed(predicate_manager_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("PE LE: Response from %s, %u, %u hops away. Failed predicate %u.\n",
		addr2str(from), packetbuf_datalen(), hops, response->predicate_id);
}

static const predicate_manager_callbacks_t pm_callbacks = { NULL, &pm_predicate_failed };


static tree_agg_conn_t aggconn;
static const tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};

static neighbour_agg_conn_t nconn;
static const neighbour_agg_callbacks_t neighbour_callbacks = {&handle_neighbour_data};

static predicate_manager_conn_t predconn;

static bool send_example_predicate(rimeaddr_t const * destination, uint8_t id)
{
	if (destination == NULL)
		return false;

	static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};
	
	static var_elem_t var_details[2];
	var_details[0].hops = 2;
	var_details[0].var_id = 255;
	var_details[1].hops = 1;
	var_details[1].var_id = 254;

	const uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
	const uint8_t var_details_length = 2;

	return predicate_manager_create(&predconn,
		id, destination,
		program_bytecode, bytecode_length,
		var_details, var_details_length);
}

PROCESS_THREAD(data_gather, ev, data)
{
	static rimeaddr_t sink;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

#ifdef NODE_ID
	node_id_burn(NODE_ID); //Burn the Node ID to the device
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL); //Set the power levels for the radio
#endif 

	// Assign the sink node, default as 1.0
	sink.u8[0] = 1;
	sink.u8[1] = 0;

	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	predicate_manager_open(&predconn, 135, 129, &sink, TRICKLE_INTERVAL, &pm_callbacks);

	if (rimeaddr_cmp(&rimeaddr_node_addr, &sink))
	{
		PEDPRINTF("PE GE: We are sink node.\n");

		predicate_manager_start_serial_input(&predconn);
	}

	// Wait for some time to let process start up and perform neighbour detect
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	// Setup the map
	unique_array_init(&neighbour_info, &rimeaddr_pair_equality, &free);

	//printf("PE GE: Starting Neighbour Aggregation\n");

	neighbour_aggregate_open(&nconn, &sink, 121, 110, 150, &neighbour_callbacks);

	// Wait for some time to let process start up and perform neighbour detect
	etimer_set(&et, 120 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));


	PEDPRINTF("PE GE: Starting Data Aggregation\n");

	tree_agg_open(&aggconn, &sink, 140, 170, sizeof(aggregation_data_t), &callbacks);


	// If sink start the evaluation process to run in the background
	if (rimeaddr_cmp(&rimeaddr_node_addr, &sink))
	{	
		map_init(&received_data, &rimeaddr_equality, &free);

		rimeaddr_t target;
		target.u8[0] = 5;
		target.u8[1] = 0;
		send_example_predicate(&target, 0);

		pred_round_count = 0;
		
		// Start the evauluation method
		while (true)
		{
			etimer_set(&et, INITIAL_ROUND_LENGTH);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

			data_evaluation();
		}

		map_free(&received_data);
	}

exit:
	tree_agg_close(&aggconn);
	neighbour_aggregate_close(&nconn);
	unique_array_free(&neighbour_info);
	predicate_manager_close(&predconn);

	PROCESS_END();
}

// This is an arbitrary difference function that the user should define
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

PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;
	static uint8_t round_count;
	static node_data_t current_data, previous_data;

	PROCESS_BEGIN();
	
	round_count = 0;

	while (true)
	{
		etimer_set(&et, ROUND_LENGTH);

		// Find the current data
		our_node_data(&current_data);

		// Usually we would have leaf nodes starting sending data back up the tree
		// instead any node may do so, but only if its data has changed.
		// However, for the first round, we only let the leaves do the sending to save on
		// energy
		if (
			(round_count == 0 && tree_agg_is_leaf(&aggconn)) ||
			(round_count > 0 && node_data_differs(&current_data, &previous_data))
		   )
		{
			// We should be set up by now
			// Start sending data up the tree

			size_t data_length = sizeof(collected_data_t) + sizeof(node_data_t);
			collected_data_t * msg = (collected_data_t *)malloc(data_length);

			msg->round_count = round_count;
			msg->length = 1;

			// Get the pointer after the message that will contain the nodes data and fill it
			node_data_t * msgdata = (node_data_t *)(msg + 1);
			memcpy(msgdata, &current_data, sizeof(node_data_t));

			// Remember the changed data
			memcpy(&previous_data, &current_data, sizeof(node_data_t));

			tree_agg_send(&aggconn, msg, data_length);

			// No need to free data as tree_agg now owns that memory
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		++round_count;
	}

	PROCESS_END();
}

static bool evaluate_predicate(
	ubyte const * program, unsigned int program_length,
	node_data_t * all_neighbour_data,
	var_elem_t const * variables, unsigned int variables_length,
	hop_data_t * hop_data)
{
	// Set up the predicate language VM
	init_pred_lang(&node_data, sizeof(node_data_t));

	// Register the data functions 
	register_function(0, &get_addr, TYPE_INTEGER);
	register_function(2, &get_temp, TYPE_FLOATING);
	register_function(3, &get_humidity, TYPE_INTEGER);

	printf("PE GE: Binding variables using %p %d\n", all_neighbour_data, variables_length);

	// Bind the variables to the VM
	unsigned int i;
	for (i = 0; i < variables_length; ++i)
	{
		// Get the length of this hop's data
		// including all of the closer hop's data length
		unsigned int length = hop_manager_length(hop_data, &variables[i]);

		printf("PE GE: Binding variables: var_id=%d hop=%d length=%d\n", variables[i].var_id, variables[i].hops, length);
		bind_input(variables[i].var_id, all_neighbour_data, length);
	}

	return evaluate(program, program_length);
}

static void data_evaluation(void)
{
	PEDPRINTF("PE GE: Eval: Beginning Evaluation\n");

	map_t const * predicate_map = predicate_manager_get_map(&predconn);

	map_elem_t elem;
	for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pred = (predicate_detail_entry_t const *)map_data(predicate_map, elem);
		
		// Copy in the simulated node
	    rimeaddr_copy(&pred_simulated_node, &pred->target);

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
				printf("PE GE: Eval: Checking Target: %s for hops %d\n", addr2str(t), hops);

				// Go through the neighbours for the node
				unique_array_elem_t neighbours_elem;
				for (neighbours_elem = unique_array_first(&neighbour_info); 
					unique_array_continue(&neighbour_info, neighbours_elem); 
					neighbours_elem = unique_array_next(neighbours_elem))
				{
					// The neighbour found
					rimeaddr_pair_t * neighbours = unique_array_data(&neighbour_info, neighbours_elem);

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
						printf("PE GE: Eval: Checking neighbour %s\n", addr2str(neighbour));

						// If the neighbour hasn't been seen before
						if (!unique_array_contains(&seen_nodes, neighbour)) 
						{
							node_data_t * nd = (node_data_t *)map_get(&received_data, neighbour);

							if (nd == NULL)
							{
								printf("PE GE: ERROR: no info on %s\n", addr2str(neighbour));
							}
							else
							{
								// Add the node to the target nodes for the next round
								unique_array_append(&acquired_nodes, rimeaddr_clone(neighbour));

								hop_manager_record(&hop_data, hops, nd, sizeof(node_data_t));
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
		node_data_t * all_neighbour_data = NULL;

		// Number of nodes we pass to the evaluation
		const unsigned int max_size = hop_manager_max_size(&hop_data);

		if (max_size > 0)
		{
			all_neighbour_data = (node_data_t *) malloc(sizeof(node_data_t) * max_size);

			// Position in all_neighbour_data
			unsigned int count = 0;

			uint8_t i;
			for (i = 1; i <= max_hops; ++i)
			{
				map_t * hop_map = hop_manager_get(&hop_data, i);

				array_list_elem_t aelem;
				for (aelem = map_first(hop_map); map_continue(hop_map, aelem); aelem = map_next(aelem))
				{
					node_data_t * mapdata = (node_data_t *)map_data(hop_map, aelem);
					memcpy(&all_neighbour_data[count], mapdata, sizeof(node_data_t));
					++count;
				}

				printf("PE GE: Eval: i=%d Count=%d/%d length=%d\n", i, count, max_size, map_length(hop_map));
			}
		}

		bool evaluation_result = evaluate_predicate(
			pred->bytecode, pred->bytecode_length,
			all_neighbour_data,
			pred->variables_details, pred->variables_details_length,
			&hop_data);

		if (evaluation_result)
		{
			printf("PE GE: Pred: TRUE\n");
		}
		else
		{
			printf("PE GE: Pred: FAILED due to error: %s\n", error_message());
		}

		predicate_manager_send_response(&predconn, &hop_data,
			pred, all_neighbour_data, sizeof(node_data_t), max_size);

		free(all_neighbour_data);

		hop_manager_reset(&hop_data);
		unique_array_free(&target_nodes);
		unique_array_free(&seen_nodes);
		unique_array_free(&acquired_nodes);
	}

	// Empty details received and let the next round fill them up
	// We do not clear received_data, as that is sent only when it changes
	unique_array_clear(&neighbour_info);

	printf("PE GE: Round: finishing=%u received_data=%u\n",
		pred_round_count, map_length(&received_data));
	
	++pred_round_count;
}
