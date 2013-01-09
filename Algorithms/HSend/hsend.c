#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "net/rime.h"
#include "contiki.h"

#include "dev/leds.h"

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "nhopreq.h"
#include "predlang.h"

#include "sensor-converter.h"
#include "debug-helper.h"

typedef struct
{
	rimeaddr_t addr;
	double temp;
	double humidity;
} node_data_t;

static void node_data(void * data)
{
	if (data != NULL)
	{
		node_data_t * nd = (node_data_t *)data;

		// Store the current nodes address
		rimeaddr_copy(&nd->addr, &rimeaddr_node_addr);

		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		nd->temp = sht11_temperature(raw_temperature);
		nd->humidity = sht11_relative_humidity_compensated(raw_humidity, nd->temp);
	}
}

static void receieved_data(rimeaddr_t const * from, uint8_t hops, void const * data)
{
	node_data_t const * nd = (node_data_t const *)data;

	char from_str[RIMEADDR_STRING_LENGTH];
	char addr_str[RIMEADDR_STRING_LENGTH];

	printf("Obtained information from %s (%s) hops:%u, T:%d H:%d%%\n",
		addr2str_r(from, from_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&nd->addr, addr_str, RIMEADDR_STRING_LENGTH),
		hops,
		(int)nd->temp, (int)nd->humidity);
}




static void const * get_addr_fn(void const * ptr)
{
	return &((node_data_t const *)ptr)->addr;
}

static void const * get_temp_fn(void const * ptr)
{
	return &((node_data_t const *)ptr)->temp;
}

static void const * get_humidity_fn(void const * ptr)
{
	return &((node_data_t const *)ptr)->humidity;
}


static void init(void)
{
	init_pred_lang(&node_data, sizeof(node_data_t));

	// Register the data functions 
	register_function("addr", &get_addr_fn, TYPE_INTEGER);
	register_function("temp", &get_temp_fn, TYPE_FLOATING);
	register_function("humidity", &get_humidity_fn, TYPE_FLOATING);
}


PROCESS(mainProcess, "HSEND Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static hsend_conn_t hc;
	static rimeaddr_t baseStationAddr, test;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// Set up the predicate language VM
	init();

	// TODO:
	// - Wait for sink to send us a Request-Evaluation message

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	// Set the id of the node that will do the testing
	test.u8[0] = 2;
	test.u8[1] = 0;

	if (!hsend_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t), &receieved_data))
	{
		printf("start function failed\n");
	}

	// 10 second timer
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	printf("Starting loops:\n");

	if (is_base(&hc)) //SINK
	{
		printf("Is the base station!\n");

		leds_on(LEDS_BLUE);

		while (true)
		{
			etimer_set(&et, 10 * CLOCK_SECOND);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
	else //NODE
	{
		static int count = 0;

		while (true)
		{
			etimer_reset(&et);

			if (rimeaddr_cmp(&rimeaddr_node_addr, &test) && count++ == 0)
			{
				printf("Sending pred req\n");

				hsend_request_info(&hc, 3);
			}

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}

	// TODO:
	// - Feed N-Hop neighbourhood info into predicate evualuator
	//   If predicate failed inform sink

exit:
	printf("Exiting Process...\n");
	hsend_end(&hc);
	PROCESS_END();
}

