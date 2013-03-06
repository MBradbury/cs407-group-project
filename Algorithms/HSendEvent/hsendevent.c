#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "contiki.h"

#include "net/rime.h"

#ifdef NODE_ID
#include "sys/node-id.h"
#endif

#include "dev/leds.h"
#include "dev/sht11-sensor.h"
#include "dev/light-sensor.h"

#include "containers/map.h"
#include "net/eventupdate.h"
#include "predlang.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "rimeadder-helpers.h"

// The custom headers we use
static const struct packetbuf_attrlist trickle_attributes[] = {
	{ PACKETBUF_ADDR_ERECEIVER, PACKETBUF_ADDRSIZE },
	TRICKLE_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};


// Struct for the list of node_data. It contains owner_addr, temperature and humidity. 
typedef struct
{
	rimeaddr_t addr;
	nfloat temp;
	nint humidity;
	//nint light1;
	//nint light2;
} node_data_t;

// Struct for the list of bytecode_variables. It contains the variable_id and hop count.
typedef struct
{
	uint8_t hops;
	uint8_t var_id;
} var_elem_t;

//Struct recieved from base station that contains a predicate to be evaluated by this node.
typedef struct 
{
	uint8_t predicate_id;
	uint8_t bytecode_length; // length of the bytecode_instructions, located after the struct
	uint8_t num_of_bytecode_var; // number of variables after the struct
} eval_pred_req_t;

typedef struct
{
	uint8_t hops;
	uint8_t var_id;
	uint8_t length;
} hops_position_t;

typedef struct
{
	uint8_t predicate_id;
	uint8_t num_hops_positions;
	uint8_t data_length;
} failure_response_t;

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
static bool node_data_differs(void * data1, void * data2)
{
	node_data_t * nd1 = (node_data_t *)data1;
	node_data_t * nd2 = (node_data_t *)data2;

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
///
/// End VM Helper Functions
///

// An array_list_t of map_t of node_data_t
static array_list_t hops_data;

// Count the number of elements added to each of the lists
static unsigned int max_size = 0;

static map_t * get_hop_map(uint8_t hop)
{
	if (hop == 0)
		return NULL;

	unsigned int length = array_list_length(&hops_data);

	// Map doesn't exist so create it
	if (length < hop)
	{
		unsigned int to_add;
		for (to_add = hop - length; to_add > 0; --to_add)
		{
			map_t * map = (map_t *)malloc(sizeof(map_t));
			map_init(map, &rimeaddr_equality, &free);

			array_list_append(&hops_data, map);
		}
	}

	return (map_t *)array_list_data(&hops_data, hop - 1);
}

static void free_hops_data(void * voiddata)
{
	map_t * data = (map_t *)voiddata;
	map_clear(data);
	free(data);
}


static void receieved_data(event_update_conn_t * c, rimeaddr_t const * from, uint8_t hops, uint8_t previous_hops)
{
	node_data_t const * nd = (node_data_t const *)packetbuf_dataptr();

	printf("Obtained information from %s hops:%u (prev:%d), T:%d H:%d%%\n",
		addr2str(from),
		hops, previous_hops,
		(int)nd->temp, (int)nd->humidity);

	/*printf("Obtained information from %s (%s) hops:%u, T:%d H:%d%% L1:%d L2:%d\n",
		addr2str_r(from, from_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&nd->addr, addr_str, RIMEADDR_STRING_LENGTH),
		hops,
		(int)nd->temp, (int)nd->humidity,
		(int)nd->light1, (int)nd->light2);*/

	// If we have previously stored data from this node at
	// a different location, then we need to forget about that
	// information
	if (previous_hops != 0 && hops != previous_hops)
	{
		map_t * prev_map = get_hop_map(previous_hops);

		map_remove(prev_map, from);
	}


	map_t * map = get_hop_map(hops);

	// Check that we have not previously received data from this node before
	node_data_t * stored = (node_data_t *)map_get(map, from);
	
	if (stored == NULL)
	{
		stored = (node_data_t *)malloc(sizeof(node_data_t));

		map_put(map, stored);
		max_size++;
	}

	memcpy(stored, nd, sizeof(node_data_t));
}



static event_update_conn_t euc;
static struct trickle_conn tcsender;
static struct mesh_conn meshreceiver;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;



typedef struct
{
	uint8_t id; // Keep id as the first variable in the struct
	uint8_t variables_details_length;
	uint8_t bytecode_length;

	rimeaddr_t target;

	var_elem_t * variables_details;
	ubyte * bytecode;

} predicate_detail_entry_t;

static void predicate_detail_entry_cleanup(void * item)
{
	predicate_detail_entry_t * entry = (predicate_detail_entry_t *)item;

	free(entry->variables_details);
	free(entry->bytecode);
	free(entry);
}

static bool predicate_id_equal(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	predicate_detail_entry_t const * l = (predicate_detail_entry_t const *)left;
	predicate_detail_entry_t const * r = (predicate_detail_entry_t const *)right;

	return l->id == r->id;
}

static uint8_t maximum_hop_data_request(var_elem_t const * variables, unsigned int length)
{
	uint8_t max_hops = 0;

	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		if (variables[i].hops > max_hops)
		{
			max_hops = variables[i].hops;
		}

		//printf("variables added: %d %d\n",varmap_cleariables[i].hops,variables[i].var_id);
	}

	return max_hops;
}

static map_t predicate_map;
static uint8_t max_comm_hops = 0;

// Rime address of target node (or rimeaddr_null for everyone)
// binary bytecode for the VM
static void trickle_send_rcv(struct trickle_conn * c)
{
	// Copy out packet, allows handling multiple evals at once
	eval_pred_req_t const * msg = (eval_pred_req_t *)packetbuf_dataptr();

	// Get eventual destination from header
	rimeaddr_t const * target = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

	//printf("Rcv packet length %d\n", packetbuf_datalen());
	
	leds_off(LEDS_GREEN);

	if (msg->bytecode_length == 0)
	{
		// There is no bytecode, so interpret this as a request to stop
		// evaluating this predicate
		map_remove(&predicate_map, &msg->predicate_id);

		printf("Removed predicate with id %d\n", msg->predicate_id);
	}
	else
	{
		// Add or update entry
		predicate_detail_entry_t * stored = (predicate_detail_entry_t *)map_get(&predicate_map, &msg->predicate_id);

		if (stored)
		{
			printf("Updating predicate with id %d.\n", msg->predicate_id);

			// Re-allocate data structures if needed

			if (msg->bytecode_length != stored->bytecode_length)
			{
				free(stored->bytecode);
				stored->bytecode = malloc(sizeof(ubyte) * msg->bytecode_length);
			}

			if (msg->num_of_bytecode_var != stored->variables_details_length)
			{
				free(stored->variables_details);
				stored->variables_details = malloc(sizeof(var_elem_t) * msg->num_of_bytecode_var);
			}
		}
		else
		{
			printf("Creating predicate with id %d.\n", msg->predicate_id);

			// Allocate memory for the data
			stored = malloc(sizeof(predicate_detail_entry_t));

			stored->id = msg->predicate_id; //set the key
			stored->bytecode = malloc(sizeof(ubyte) * msg->bytecode_length);
			stored->variables_details = malloc(sizeof(var_elem_t) * msg->num_of_bytecode_var);

			// Put data in the map
			map_put(&predicate_map, stored);
		}

		// Update the target of this predicate
		rimeaddr_copy(&stored->target, target);

		// Pointer for bytecode variables
		var_elem_t const * variables = (var_elem_t const *)(msg + 1);

		// Create a pointer to the bytecode instructions stored in the message.
		ubyte const * bytecode_instructions = (ubyte const *)(variables + msg->num_of_bytecode_var);

		// Update data
		stored->bytecode_length = msg->bytecode_length;
		stored->variables_details_length = msg->num_of_bytecode_var;

		memcpy(stored->bytecode, bytecode_instructions, sizeof(ubyte) * stored->bytecode_length);
		memcpy(stored->variables_details, variables, sizeof(var_elem_t) * stored->variables_details_length);
	}

	leds_off(LEDS_RED);

	// We need to find and set the maximum distance of all predicates
	map_elem_t elem;
	for (elem = map_first(&predicate_map); map_continue(&predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(&predicate_map, elem);

		uint8_t local_max_hops = maximum_hop_data_request(pe->variables_details, pe->variables_details_length);

		if (local_max_hops > max_comm_hops)
		{
			max_comm_hops = local_max_hops;
		}

		// Set the led to be red if this node will evaluate a predicate
		if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
		{
			leds_on(LEDS_RED);
		}
	}

	event_update_set_distance(&euc, max_comm_hops);
}

// Used to handle receiving predicate failure messages
static void mesh_receiver_rcv(struct mesh_conn *c, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("Response received from %s, %u hops away. ", addr2str(from), hops);
	printf("Failed predicate %u.\n", response->predicate_id);
}

static void mesh_receiver_sent(struct mesh_conn * c)
{
}

static void mesh_receiver_timeout(struct mesh_conn * c)
{
	printf("Mesh timedout, so start sending again\n");
	// We timedout, so start sending again
	mesh_send(&meshreceiver, &baseStationAddr);
}

static const struct trickle_callbacks tcsender_callbacks = { &trickle_send_rcv };

static const struct mesh_callbacks meshreceiver_callbacks = { &mesh_receiver_rcv, &mesh_receiver_sent, &mesh_receiver_timeout };

PROCESS(mainProcess, "MAIN Process");
PROCESS(hsendProcess, "HSEND Process");

AUTOSTART_PROCESSES(&mainProcess, &hsendProcess);


static bool send_example_predicate(rimeaddr_t const * destination, uint8_t id)
{
	if (destination == NULL)
		return false;

	static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};

	// Send the request message

	uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
	uint8_t var_details = 2;
	
	unsigned int packet_size = sizeof(eval_pred_req_t) + bytecode_length + (sizeof(var_elem_t) * var_details);

	packetbuf_clear();
	packetbuf_set_datalen(packet_size);
	debug_packet_size(packet_size);
	eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();
	memset(msg, 0, packet_size);

	// Set eventual destination in header
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, destination);

	msg->predicate_id = id;
	msg->bytecode_length = bytecode_length;
	msg->num_of_bytecode_var = var_details;

	var_elem_t * msg_vars = (var_elem_t *)(msg + 1);

	msg_vars[0].hops = 2;
	msg_vars[0].var_id = 255;
	msg_vars[1].hops = 1;
	msg_vars[1].var_id = 254;

	ubyte * msg_bytecode = (ubyte *)(msg_vars + var_details);

	// Debug check to make sure that we have done sane things!
	if ((void *)(msg_bytecode + bytecode_length) - (void *)msg != packet_size)
	{
		printf("Failed to copy data correctly got=%d expected=%u!\n",
			(void *)(msg_bytecode + bytecode_length) - (void *)msg,
			packet_size);
	}

	memcpy(msg_bytecode, program_bytecode, bytecode_length);

	//printf("Sent packet length %d\n", packet_size);

	trickle_send(&tcsender);

	return true;
}

static bool send_cancel_predicate(rimeaddr_t const * destination, uint8_t id)
{
	if (destination == NULL)
		return false;

	unsigned int packet_size = sizeof(eval_pred_req_t);

	packetbuf_clear();
	packetbuf_set_datalen(packet_size);
	debug_packet_size(packet_size);
	eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();
	memset(msg, 0, packet_size);

	// Set eventual destination in header
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, destination);

	msg->predicate_id = id;

	// Setting bytecode length to 0 indicates that this predicate should be removed
	msg->bytecode_length = 0;
	msg->num_of_bytecode_var = 0;

	trickle_send(&tcsender);

	return true;
}

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;
	static rimeaddr_t destination;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

#ifdef NODE_ID
	node_id_burn(NODE_ID);
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL);
#endif

	array_list_init(&hops_data, &free_hops_data);

	map_init(&predicate_map, &predicate_id_equal, &predicate_detail_entry_cleanup);

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	// Set the predicate evaluation target
	destination.u8[0] = 5;
	destination.u8[1] = 0;

	if (rimeaddr_cmp(&rimeaddr_node_addr,&destination))
	{
		printf("Is Destination.\n");
	}

	trickle_open(&tcsender, trickle_interval, 121, &tcsender_callbacks);
	channel_set_attributes(121, trickle_attributes);

	mesh_open(&meshreceiver, 126, &meshreceiver_callbacks);

	if (!event_update_start(&euc, 149, &node_data, &node_data_differs, sizeof(node_data_t), CLOCK_SECOND * 10, &receieved_data))
	{
		printf("nhopreq start function failed\n");
	}

	if (rimeaddr_cmp(&baseStationAddr, &rimeaddr_node_addr)) // Sink
	{
		printf("Is the base station!\n");

		send_example_predicate(&destination, 0);
		send_example_predicate(&destination, 1);

		leds_on(LEDS_BLUE);

		while (true)
		{
			etimer_set(&et, 10 * CLOCK_SECOND);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
	else
	{
		leds_on(LEDS_GREEN);

		while (true)
		{
			etimer_set(&et, 10 * CLOCK_SECOND);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}	

exit:
	printf("Exiting MAIN Process...\n");
	array_list_clear(&hops_data);
	event_update_stop(&euc);
	trickle_close(&tcsender);
	mesh_close(&meshreceiver);
	PROCESS_END();
}

static uint8_t hop_data_length(var_elem_t const * variable)
{
	unsigned int length = 0;
	uint8_t j;
	for (j = 1; j <= variable->hops; ++j)
	{
		length += map_length(get_hop_map(j));
	}

	return length;
}


static bool evaluate_predicate(
	ubyte const * program, unsigned int program_length,
	node_data_t const * all_neighbour_data,
	var_elem_t const * variables, unsigned int variables_length)
{
	// Set up the predicate language VM
	init_pred_lang(&node_data, sizeof(node_data_t));

	// Register the data functions 
	register_function(0, &get_addr, TYPE_INTEGER);
	register_function(2, &get_temp, TYPE_FLOATING);
	register_function(3, &get_humidity, TYPE_INTEGER);

	// Bind the variables to the VM
	unsigned int i;
	for (i = 0; i < variables_length; ++i)
	{
		// Get the length of this hop's data
		// including all of the closer hop's data length
		unsigned int length = hop_data_length(&variables[i]);

		printf("Binding variables: var_id=%d hop=%d length=%d\n", variables[i].var_id, variables[i].hops, length);
		bind_input(variables[i].var_id, all_neighbour_data, length);
	}

	return evaluate(program, program_length);
}

PROCESS_THREAD(hsendProcess, ev, data)
{
	static struct etimer et;
	static node_data_t * all_neighbour_data = NULL;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	printf("HSEND Process Started.\n");

	// Wait for other nodes to initialize.
	etimer_set(&et, 20 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	while (true)
	{
		printf("HSEND: Starting long wait...\n");

		etimer_set(&et, 5 * 60 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf("HSEND: Wait finished! About to ask for data!\n");
	

		// Only ask for data if the predicate needs it
		if (max_comm_hops != 0)
		{
			// Generate array of all the data
			all_neighbour_data = (node_data_t *) malloc(sizeof(node_data_t) * max_size);

			uint8_t i;
			for (i = 1; i <= max_comm_hops; ++i)
			{
				map_t * hop_map = get_hop_map(i);

				unsigned int length = map_length(hop_map);

				// Position in all_neighbour_data
				unsigned int count = 0;

				if (length > 0)
				{
					map_elem_t elem;
					for (elem = map_first(hop_map); map_continue(hop_map, elem); elem = map_next(elem))
					{
						node_data_t * mapdata = (node_data_t *)map_data(hop_map, elem);
						memcpy(&all_neighbour_data[count], mapdata, sizeof(node_data_t));
						++count;
					}
				}

				printf("i=%d Count=%d length=%d\n", i, count, length);
			}
		}

		map_elem_t elem;
		for (elem = map_first(&predicate_map); map_continue(&predicate_map, elem); elem = map_next(elem))
		{
			predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(&predicate_map, elem);

			if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
			{
				printf("Starting predicate evaluation of %d with code length: %d.\n", pe->id, pe->bytecode_length);
	
				bool evaluation_result = evaluate_predicate(
					pe->bytecode, pe->bytecode_length,
					all_neighbour_data,
					pe->variables_details, pe->variables_details_length);

				if (evaluation_result)
				{
					printf("Pred: TRUE\n");
				}
				else
				{
					printf("Pred: FAILED due to error: %s\n", error_message());
				}

				unsigned int packet_length = sizeof(failure_response_t) +
											 sizeof(hops_position_t) * pe->variables_details_length +
											 sizeof(node_data_t) * max_size;

				packetbuf_clear();
				packetbuf_set_datalen(packet_length);
				debug_packet_size(packet_length);
				failure_response_t * msg = (failure_response_t *)packetbuf_dataptr();
				memset(msg, 0, packet_length);

				msg->predicate_id = pe->id;
				msg->num_hops_positions = pe->variables_details_length;
				msg->data_length = max_size;

				hops_position_t * hops_details = (hops_position_t *)(msg + 1);

				unsigned int i;
				for (i = 0; i < msg->num_hops_positions; ++i)
				{
					hops_details[i].hops = pe->variables_details[i].hops;
					hops_details[i].var_id = pe->variables_details[i].var_id;
					hops_details[i].length = hop_data_length(&pe->variables_details[i]);
				}

				node_data_t * msg_neighbour_data = (node_data_t *)(hops_details + pe->variables_details_length);

				memcpy(msg_neighbour_data, all_neighbour_data, sizeof(node_data_t) * max_size);

				mesh_send(&meshreceiver, &baseStationAddr);
			}
		}

		// Free the allocated neighbour data
		free(all_neighbour_data);
		all_neighbour_data = NULL;
	}

exit:
	printf("Exiting HSEND Process...\n");
	PROCESS_END();
}

