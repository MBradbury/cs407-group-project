#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "contiki.h"

#include "sys/node-id.h"

#include "dev/leds.h"
#include "dev/sht11-sensor.h"
#include "dev/light-sensor.h"

#include "containers/map.h"
#include "net/eventupdate.h"
#include "net/rimeaddr-helpers.h"
#include "predicate-manager.h"
#include "hop-data-manager.h"
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
///
/// End VM Helper Functions
///

static hop_data_t hop_data;

static void receieved_data(event_update_conn_t * c, rimeaddr_t const * from, uint8_t hops, uint8_t previous_hops)
{
	node_data_t const * nd = (node_data_t const *)packetbuf_dataptr();

	printf("PE LE: Obtained information from %s hops:%u (prev:%d), T:%d H:%d%%\n",
		addr2str(from),
		hops, previous_hops,
		(int)nd->temp, (int)nd->humidity);

	/*printf("PE LE: Obtained information from %s (%s) hops:%u, T:%d H:%d%% L1:%d L2:%d\n",
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
		hop_manager_remove(&hop_data, previous_hops, from);
	}

	hop_manager_record(&hop_data, hops, nd, sizeof(node_data_t));
}



static event_update_conn_t euc;
static predicate_manager_conn_t predconn;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;


static uint8_t max_comm_hops = 0;

static void pm_update_callback(struct predicate_manager_conn * conn)
{
	map_t const * predicate_map = predicate_manager_get_map(conn);

	max_comm_hops = 0;

	// We need to find and set the maximum distance of all predicates
	map_elem_t elem;
	for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(predicate_map, elem);

		uint8_t local_max_hops = predicate_manager_max_hop(pe);

		if (local_max_hops > max_comm_hops)
		{
			max_comm_hops = local_max_hops;
		}
	}

	event_update_set_distance(&euc, max_comm_hops);
}

static void pm_predicate_failed(predicate_manager_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("PE LE: Response received from %s, %u, %u hops away. ",
		addr2str(from), packetbuf_datalen(), hops);

	printf("Failed predicate %u.\n", response->predicate_id);
}

static const predicate_manager_callbacks_t pm_callbacks = { &pm_update_callback, &pm_predicate_failed };

PROCESS(mainProcess, "Main Process");

AUTOSTART_PROCESSES(&mainProcess);


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
		unsigned int length = hop_manager_length(&hop_data, &variables[i]);

		printf("PE LE: Binding variables: var_id=%d hop=%d length=%d\n", variables[i].var_id, variables[i].hops, length);
		bind_input(variables[i].var_id, all_neighbour_data, length);
	}

	return evaluate(program, program_length);
}

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;
	static node_data_t * all_neighbour_data = NULL;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	printf("PE LE: Process Started.\n");

	// Init code
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

	predicate_manager_open(&predconn, 121, 126, &baseStationAddr, trickle_interval, &pm_callbacks);

	if (!event_update_start(&euc, 149, &node_data, &node_data_differs, sizeof(node_data_t), CLOCK_SECOND * 10, &receieved_data))
	{
		printf("PE LE: nhopreq start function failed\n");
	}

	if (rimeaddr_cmp(&baseStationAddr, &rimeaddr_node_addr)) // Sink
	{
		printf("PE LE: Is the base station!\n");

		// As we are the base station we need to start reading the serial input
		predicate_manager_start_serial_input(&predconn);

		// Set the predicate evaluation target
		rimeaddr_t destination;
		destination.u8[0] = 5;
		destination.u8[1] = 0;

		if (rimeaddr_cmp(&rimeaddr_node_addr, &destination))
		{
			printf("PE LE: Is Destination.\n");
		}

		send_example_predicate(&destination, 0);
		send_example_predicate(&destination, 1);

		leds_on(LEDS_BLUE);
	}
	else
	{
		leds_on(LEDS_GREEN);
	}	
	// Init end

	// Wait for other nodes to initialize.
	etimer_set(&et, 20 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	while (true)
	{
		printf("PE LE: Starting long wait...\n");

		etimer_set(&et, 5 * 60 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf("PE LE: Wait finished!\n");
	
		const unsigned int max_size = hop_manager_max_size(&hop_data);

		// Only ask for data if the predicate needs it
		if (max_comm_hops != 0 && max_size > 0)
		{
			// Generate array of all the data
			all_neighbour_data = (node_data_t *) malloc(sizeof(node_data_t) * max_size);

			// Position in all_neighbour_data
			unsigned int count = 0;

			uint8_t i;
			for (i = 1; i <= max_comm_hops; ++i)
			{
				map_t * hop_map = hop_manager_get(&hop_data, i);

				map_elem_t elem;
				for (elem = map_first(hop_map); map_continue(hop_map, elem); elem = map_next(elem))
				{
					node_data_t * mapdata = (node_data_t *)map_data(hop_map, elem);
					memcpy(&all_neighbour_data[count], mapdata, sizeof(node_data_t));
					++count;
				}

				printf("PE LE: i=%d Count=%d/%d length=%d\n", i, count, max_size, map_length(hop_map));
			}
		}

		map_t const * predicate_map = predicate_manager_get_map(&predconn);

		map_elem_t elem;
		for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
		{
			predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(predicate_map, elem);

			if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
			{
				printf("PE LE: Starting predicate evaluation of %d with code length: %d.\n", pe->id, pe->bytecode_length);
	
				bool evaluation_result = evaluate_predicate(
					pe->bytecode, pe->bytecode_length,
					all_neighbour_data,
					pe->variables_details, pe->variables_details_length);

				if (evaluation_result)
				{
					printf("PE LE: Pred: TRUE\n");
				}
				else
				{
					printf("PE LE: Pred: FAILED due to error: %s\n", error_message());
				}

				predicate_manager_send_response(&predconn, &hop_data,
					pe, all_neighbour_data, sizeof(node_data_t), max_size);
			}
		}

		// Free the allocated neighbour data
		free(all_neighbour_data);
		all_neighbour_data = NULL;
	}

exit:
	printf("PE LE: Exiting Process...\n");
	hop_manager_free(&hop_data);
	event_update_stop(&euc);
	predicate_manager_close(&predconn);
	PROCESS_END();
}

