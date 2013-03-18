#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "contiki.h"

#include "net/rime.h"

#include "sys/node-id.h"

#include "dev/leds.h"
#include "dev/sht11-sensor.h"
#include "dev/light-sensor.h"

#include "containers/map.h"
#include "net/rimeaddr-helpers.h"
#include "predicate-manager.h"
#include "hop-data-manager.h"
#include "nhopreq.h"
#include "predlang.h"
#include "sensor-converter.h"
#include "debug-helper.h"

// Struct for the list of node_data. It contains owner_addr, temperature and humidity. 
typedef struct
{
	rimeaddr_t addr;
	nfloat temp;
	nint humidity;
	//nint light1;
	//nint light2;
} node_data_t;

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
///
/// End VM Helper Functions
///

static hop_data_t hop_data;

static void receieved_data(rimeaddr_t const * from, uint8_t hops, void const * data)
{
	node_data_t const * nd = (node_data_t const *)data;

	char from_str[RIMEADDR_STRING_LENGTH];
	char addr_str[RIMEADDR_STRING_LENGTH];

	printf("PE LP: Obtained information from %s (%s) hops:%u, T:%d H:%d%%\n",
		addr2str_r(from, from_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&nd->addr, addr_str, RIMEADDR_STRING_LENGTH),
		hops,
		(int)nd->temp, (int)nd->humidity);

	/*printf("PE LP: Obtained information from %s (%s) hops:%u, T:%d H:%d%% L1:%d L2:%d\n",
		addr2str_r(from, from_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&nd->addr, addr_str, RIMEADDR_STRING_LENGTH),
		hops,
		(int)nd->temp, (int)nd->humidity,
		(int)nd->light1, (int)nd->light2);*/

	hop_manager_record(&hop_data, hops, nd, sizeof(node_data_t));
}



static nhopreq_conn_t hc;
static predicate_manager_conn_t predconn;
static struct mesh_conn meshreceiver;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;

static uint8_t max_hops = 0;

static void predicate_manager_update_callback(struct predicate_manager_conn * conn)
{
	map_t const * predicate_map = predicate_manager_get_map(conn);

	max_hops = 0;

	map_elem_t elem;
	for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(predicate_map, elem);

		if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
		{
			uint8_t local_max_hops = predicate_manager_max_hop(pe);

			if (local_max_hops > max_hops)
			{
				max_hops = local_max_hops;
			}
		}
	}
}

// Used to handle receiving predicate failure messages
static void mesh_receiver_rcv(struct mesh_conn *c, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("PE LP: Response received from %s, %u hops away. ", addr2str(from), hops);
	printf("Failed predicate %u.\n", response->predicate_id);
}

static void mesh_receiver_sent(struct mesh_conn * c)
{
}

static void mesh_receiver_timeout(struct mesh_conn * c)
{
	printf("PE LP: Mesh timedout, so start sending again\n");

	// We timedout, so start sending again
	mesh_send(&meshreceiver, &baseStationAddr);
}

static const struct mesh_callbacks meshreceiver_callbacks = { &mesh_receiver_rcv, &mesh_receiver_sent, &mesh_receiver_timeout };

PROCESS(initProcess, "Init Process");
PROCESS(mainProcess, "main Process");

AUTOSTART_PROCESSES(&initProcess, &mainProcess);


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

	uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
	uint8_t var_details_length = 2;

	return predicate_manager_create(&predconn,
		id, destination,
		program_bytecode, bytecode_length,
		var_details, var_details_length);
}

PROCESS_THREAD(initProcess, ev, data)
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

	hop_manager_init(&hop_data);

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	// Set the predicate evaluation target
	destination.u8[0] = 5;
	destination.u8[1] = 0;

	if (rimeaddr_cmp(&rimeaddr_node_addr, &destination))
	{
		printf("PE LP: Is Destination.\n");
	}
	
	predicate_manager_open(&predconn, 121, trickle_interval, &predicate_manager_update_callback);

	mesh_open(&meshreceiver, 126, &meshreceiver_callbacks);

	if (!nhopreq_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t), &receieved_data))
	{
		printf("PE LP: nhopreq start function failed\n");
	}

	if (rimeaddr_cmp(&baseStationAddr, &rimeaddr_node_addr)) // Sink
	{
		printf("PE LP: Is the base station!\n");

		// As we are the base station we need to start reading the serial input
		predicate_manager_start_serial_input(&predconn);

		send_example_predicate(&destination, 0);
		send_example_predicate(&destination, 1);

		leds_on(LEDS_BLUE);
	}
	else
	{
		leds_on(LEDS_GREEN);
	}

exit:
	printf("PE LP: Exiting init Process.\n");
	/*hop_manager_free(&hop_data);
	nhopreq_end(&hc);
	predicate_manager_close(&predconn);
	mesh_close(&meshreceiver);*/

	PROCESS_END();
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

	printf("PE LP: Binding variables using %p\n", all_neighbour_data);

	// Bind the variables to the VM
	unsigned int i;
	for (i = 0; i < variables_length; ++i)
	{
		// Get the length of this hop's data
		// including all of the closer hop's data length
		unsigned int length = hop_manager_length(&hop_data, &variables[i]);

		printf("PE LP: Binding variables: var_id=%d hop=%d length=%d\n", variables[i].var_id, variables[i].hops, length);
		bind_input(variables[i].var_id, all_neighbour_data, length);
	}

	return evaluate(program, program_length);
}

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;
	static node_data_t * all_neighbour_data = NULL;
	static map_t const * predicate_map = NULL;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	printf("PE LP: Process Started.\n");

	//Wait for other nodes to initialize.
	etimer_set(&et, 20 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	while (true)
	{
		printf("PE LP: Starting long wait...\n");

		etimer_set(&et, 5 * 60 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf("PE LP: Wait finished! About to ask for data!\n");

		// Only ask for data if the predicate needs it
		if (max_hops != 0)
		{
			printf("PE LP: Starting request for %d hops of data...\n", max_hops);

			nhopreq_request_info(&hc, max_hops);
	
			// Get as much information as possible within a given time bound
			etimer_set(&et, 120 * CLOCK_SECOND);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

			printf("PE LP: Finished collecting hop data.\n");

			const unsigned int max_size = hop_manager_max_size(&hop_data);

			if (max_size > 0)
			{
				// Generate array of all the data
				all_neighbour_data = (node_data_t *) malloc(sizeof(node_data_t) * max_size);

				// Position in all_neighbour_data
				unsigned int count = 0;

				uint8_t i;
				for (i = 1; i <= max_hops; ++i)
				{
					map_t * hop_map = hop_manager_get(&hop_data, i);

					map_elem_t elem;
					for (elem = map_first(hop_map); map_continue(hop_map, elem); elem = map_next(elem))
					{
						node_data_t * mapdata = (node_data_t *)map_data(hop_map, elem);
						memcpy(&all_neighbour_data[count], mapdata, sizeof(node_data_t));
						++count;
					}

					printf("PE LP: i=%d Count=%d/%d length=%d\n", i, count, max_size, map_length(hop_map));
				}
			}
		}

		const unsigned int max_size = hop_manager_max_size(&hop_data);

		predicate_map = predicate_manager_get_map(&predconn);

		map_elem_t elem;
		for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
		{
			predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(predicate_map, elem);

			if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
			{
				printf("PE LP: Starting predicate evaluation of %d with code length: %d.\n", pe->id, pe->bytecode_length);
		
				bool evaluation_result = evaluate_predicate(
					pe->bytecode, pe->bytecode_length,
					all_neighbour_data,
					pe->variables_details, pe->variables_details_length);

				if (evaluation_result)
				{
					printf("PE LP: Pred: TRUE\n");
				}
				else
				{
					printf("PE LP: Pred: FAILED due to error: %s\n", error_message());
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
					hops_details[i].length = hop_manager_length(&hop_data, &pe->variables_details[i]);
				}

				node_data_t * msg_neighbour_data = (node_data_t *)(hops_details + pe->variables_details_length);

				memcpy(msg_neighbour_data, all_neighbour_data, sizeof(node_data_t) * max_size);

				mesh_send(&meshreceiver, &baseStationAddr);
			}
		}

		// Free the allocated neighbour data
		free(all_neighbour_data);
		all_neighbour_data = NULL;

		// We want to remove all the data we gathered,
		// this is important to do so as if a node dies
		// we do not want to keep using its last piece of data
		// we want that lack of data to be picked up on.
		hop_manager_reset(&hop_data);
	}

exit:
	printf("PE LP: Exiting Process...\n");
	PROCESS_END();
}
