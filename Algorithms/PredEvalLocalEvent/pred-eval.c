#include "pele.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "contiki.h"

#include "lib/random.h"

#include "sys/node-id.h"

#include "dev/leds.h"
#include "dev/sht11-sensor.h"
#include "dev/light-sensor.h"

#include "containers/map.h"
#include "net/rimeaddr-helpers.h"
#include "predlang.h"
#include "sensor-converter.h"
#include "debug-helper.h"

#ifdef PE_DEBUG
#	define PEDPRINTF(...) printf(__VA_ARGS__)
#else
#	define PEDPRINTF(...)
#endif

#define trickle_interval ((clock_time_t)2 * CLOCK_SECOND)
#define EVENT_CHECK_PERIOD ((clock_time_t)30 * CLOCK_SECOND)
#define CHANCE_OF_EVENT_UPDATE 0.01

#define NODE_DATA_INDEX(array, index, size) \
	(((char *)array) + ((index) * (size)))

static inline pele_conn_t * conncvt_event_update(event_update_conn_t * conn)
{
	return (pele_conn_t *)conn;
}

static inline pele_conn_t * conncvt_predicate_manager(predicate_manager_conn_t * conn)
{
	return (pele_conn_t *)
		(((char *)conn) - sizeof(event_update_conn_t));
}

static inline pele_conn_t * conncvt_hop_data(hop_data_t * conn)
{
	return (pele_conn_t *)
		(((char *)conn) - sizeof(event_update_conn_t) - sizeof(predicate_manager_conn_t));
}


static void receieved_data(event_update_conn_t * c, rimeaddr_t const * from, uint8_t hops, uint8_t previous_hops)
{
	pele_conn_t * pele = conncvt_event_update(c);

	void * nd = packetbuf_dataptr();

	PEDPRINTF("PELE: Obtained information from %s hops:%u (prev:%d)\n",
		addr2str(from), hops, previous_hops);

	// If we have previously stored data from this node at
	// a different location, then we need to forget about that
	// information
	if (previous_hops != 0 && hops != previous_hops)
	{
		hop_manager_remove(&pele->hop_data, previous_hops, from);
	}

	hop_manager_record(&pele->hop_data, hops, nd, pele->data_size);
}

static void pm_update_callback(struct predicate_manager_conn * conn)
{
	pele_conn_t * pele = conncvt_predicate_manager(conn);

	map_t const * predicate_map = predicate_manager_get_map(conn);

	pele->max_comm_hops = 0;

	// We need to find and set the maximum distance of all predicates
	map_elem_t elem;
	for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(predicate_map, elem);

		uint8_t local_max_hops = predicate_manager_max_hop(pe);

		if (local_max_hops > pele->max_comm_hops)
		{
			pele->max_comm_hops = local_max_hops;
		}
	}

	event_update_set_distance(&pele->euc, pele->max_comm_hops);
}

static void pm_predicate_failed(predicate_manager_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	pele_conn_t * pele = conncvt_predicate_manager(conn);

	pele->predicate_failed(pele, from, hops);
}

static const predicate_manager_callbacks_t pm_callbacks = { &pm_update_callback, &pm_predicate_failed };

PROCESS(pele_process, "PELE Process");
PROCESS_THREAD(pele_process, ev, data)
{
	static pele_conn_t * pele;
	static struct etimer et;
	static void * all_neighbour_data = NULL;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	pele = (pele_conn_t *)data;
	
	PEDPRINTF("PELE: Process Started.\n");

	// Wait for other nodes to initialize.
	etimer_set(&et, 20 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	while (true)
	{
		PEDPRINTF("PELE: Starting long wait...\n");

		etimer_set(&et, pele->predicate_period);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		PEDPRINTF("PELE: Wait finished!\n");
	
		const unsigned int max_size = hop_manager_max_size(&pele->hop_data);

		// Only ask for data if the predicate needs it
		if (pele->max_comm_hops != 0 && max_size > 0)
		{
			// Generate array of all the data
			all_neighbour_data = malloc(pele->data_size * max_size);

			// Position in all_neighbour_data
			unsigned int count = 0;

			uint8_t i;
			for (i = 1; i <= pele->max_comm_hops; ++i)
			{
				map_t * hop_map = hop_manager_get(&pele->hop_data, i);

				map_elem_t elem;
				for (elem = map_first(hop_map); map_continue(hop_map, elem); elem = map_next(elem))
				{
					void * mapdata = map_data(hop_map, elem);
					memcpy(NODE_DATA_INDEX(all_neighbour_data, count, pele->data_size), mapdata, pele->data_size);
					++count;
				}

				PEDPRINTF("PELE: i=%d Count=%d/%d length=%d\n", i, count, max_size, map_length(hop_map));
			}
		}

		map_t const * predicate_map = predicate_manager_get_map(&pele->predconn);

		map_elem_t elem;
		for (elem = map_first(predicate_map); map_continue(predicate_map, elem); elem = map_next(elem))
		{
			predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(predicate_map, elem);

			if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
			{
				PEDPRINTF("PELE: Starting predicate evaluation of %d with code length: %d.\n", pe->id, pe->bytecode_length);
	
				bool evaluation_result = evaluate_predicate(&pele->predconn,
					pele->data_fn, pele->data_size,
					pele->function_details, pele->functions_count,
					&pele->hop_data,
					all_neighbour_data, max_size, pe);

				if (evaluation_result)
				{
					PEDPRINTF("PELE: Pred: TRUE\n");
				}
				else
				{
					PEDPRINTF("PELE: Pred: FAILED (%s)\n", error_message());
				}
			}
		}

		// Free the allocated neighbour data
		free(all_neighbour_data);
		all_neighbour_data = NULL;
	}

exit:
	(void)0;
	PROCESS_END();
}



bool pele_start(pele_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pele_data_differs_fn differs_fn, pele_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count,
	clock_time_t predicate_period)
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
	conn->max_comm_hops = 0;
	conn->predicate_failed = predicate_failed;

	conn->function_details = function_details;
	conn->functions_count = functions_count;

	conn->predicate_period = predicate_period;

	hop_manager_init(&conn->hop_data);

	predicate_manager_open(&conn->predconn, 121, 126, sink, trickle_interval, &pm_callbacks);

	if (!event_update_start(
			&conn->euc, 149, data_fn, differs_fn,
			data_size, EVENT_CHECK_PERIOD, &receieved_data,
			CHANCE_OF_EVENT_UPDATE)
		)
	{
		PEDPRINTF("PELE: nhopreq start function failed\n");
	}

	if (rimeaddr_cmp(sink, &rimeaddr_node_addr)) // Sink
	{
		PEDPRINTF("PELE: Is the base station!\n");

		// As we are the base station we need to start reading the serial input
		predicate_manager_start_serial_input(&conn->predconn);

		leds_on(LEDS_BLUE);
	}
	else
	{
		leds_on(LEDS_GREEN);
	}

	process_start(&pele_process, (void *)conn);

	return true;
}

void pele_stop(pele_conn_t * conn)
{
	if (conn != NULL)
	{
		process_exit(&pele_process);

		hop_manager_free(&conn->hop_data);
		event_update_stop(&conn->euc);
		predicate_manager_close(&conn->predconn);
	}
}



#ifdef PELE_APPLICATION

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


static bool send_example_predicate(pele_conn_t * pele, rimeaddr_t const * destination, uint8_t id)
{
	if (pele == NULL || destination == NULL)
		return false;

	static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};
	
	static var_elem_t var_details[2];
	var_details[0].hops = 2;
	var_details[0].var_id = 255;
	var_details[1].hops = 1;
	var_details[1].var_id = 254;

	uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
	uint8_t var_details_length = 2;

	return predicate_manager_create(&pele->predconn,
		id, destination,
		program_bytecode, bytecode_length,
		var_details, var_details_length);
}

static void predicate_failed(pele_conn_t * conn, rimeaddr_t const * from, uint8_t hops)
{
	failure_response_t * response = (failure_response_t *)packetbuf_dataptr();

	printf("PELE: Response received from %s, %u, %u hops away. Failed predicate %u.\n",
		addr2str(from), packetbuf_datalen(), hops, response->predicate_id);
}

PROCESS(mainProcess, "Main Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static rimeaddr_t sink;
	static pele_conn_t pele;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	printf("PELE: Process Started.\n");

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

	pele_start(&pele,
		&sink, &node_data, sizeof(node_data_t), &node_data_differs, &predicate_failed,
		func_det, sizeof(func_det)/sizeof(func_det[0]),
		4 * 60 * CLOCK_SECOND);

	if (rimeaddr_cmp(&sink, &rimeaddr_node_addr))
	{
		rimeaddr_t destination;
		destination.u8[0] = 5;
		destination.u8[1] = 0;

		send_example_predicate(&pele, &destination, 0);
	}

	// This is where the application would be
	while (true)
	{
		// Wait for other nodes to initialize.
		etimer_set(&et, 20 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

exit:
	pele_stop(&pele);

	PROCESS_END();
}

#endif
