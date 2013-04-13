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

	static ubyte const program_bytecode[] = {
		0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,
		0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,
		0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,
		0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,
		0x2c,0x35,0x01,0x12,0x0a,0x00
	};
	
	static const var_elem_t var_details[2] = {
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
