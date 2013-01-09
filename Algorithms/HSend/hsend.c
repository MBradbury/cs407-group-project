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


static struct trickle_conn tc;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;


void trickle_rcv(struct trickle_conn * c)
{
	if (rimeaddr_cmp(&baseStationAddr, &rimeaddr_node_addr) != 0) // Sink
	{
	}
	else
	{
		// Start HSEND
		// TODO: pass arguments from trickle message to HSEND
		process_start(hsendProcess, NULL);
	}
}

static const trickle_callbacks callbacks = { &trickle_rcv };


PROCESS(mainProcess, "MAIN Process");
PROCESS(hsendProcess, "HSEND Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static hsend_conn_t hc;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	trickle_open(&tc, trickle_interval, 121, &callbacks);

	if (rimeaddr_cmp(&baseStationAddr, &rimeaddr_node_addr) != 0) // Sink
	{
		printf("Is the base station!\n");

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
	trickle_close(&tc);
	PROCESS_END();
}


PROCESS_THREAD(hsendProcess, ev, data)
{
	static hsend_conn_t hc;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	if (!hsend_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t), &receieved_data))
	{
		printf("start function failed\n");
	}

	// 10 second timer
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	// TODO:
	// Work out how many hops of information is being requested
	unsigned int hops = 2;

	printf("Sending pred req\n");

	hsend_request_info(&hc, hop);

	// Get as much information as possible within a given time bound
	etimer_set(&et, 60 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	// Set up the predicate language VM
	init();

	// TODO:
	// - Feed N-Hop neighbourhood info into predicate evualuator
	//   If predicate failed inform sink
	

exit:
	printf("Exiting HSEND Process...\n");
	hsend_end(&hc);
	PROCESS_END();
}

