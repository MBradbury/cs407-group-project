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


// Struct for the list elements, used to hold the variable names and their bytecode symbols
typedef struct var_elem_t
{
	int hops;
	char * var_id;
} var_elem_t;

typedef struct values_list_elem
{
	struct list_elem_struct * next;
	node_data_t data;
} values_list_elem;

//struct recieved from a trickle message
typedef struct 
{
	rimeaddr_t target;
	int bytecode_length; //length of the bytecode, located after the struct
	int num_of_bytecode_var; //number of variables after the struct
} eval_pred_req_t;

typedef struct
{
	rimeaddr_t addr;
	double temp;
	double humidity;
} node_data_t;

var_elem_t[] variables; //array of the variables from bytecode
values_list_elem[] hops_data;

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
	nd = malloc(sizeof(node_data_t));
	&nd->humidity = &data->humidity;
	&nd->temp = &data->temp;
	rimeaddr_copy(&nd->from,&data->from);

	char from_str[RIMEADDR_STRING_LENGTH];
	char addr_str[RIMEADDR_STRING_LENGTH];

	printf("Obtained information from %s (%s) hops:%u, T:%d H:%d%%\n",
		addr2str_r(from, from_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&nd->addr, addr_str, RIMEADDR_STRING_LENGTH),
		hops,
		(int)nd->temp, (int)nd->humidity);

	list_push(hops_data[hops-1], nd);
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

static hsend_conn_t hc;
static struct trickle_conn tc;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;

//Rime adress of target node (or null for everyone)
//binary bytecode for the VM
void trickle_rcv(struct trickle_conn * c)
{
	//might have to copy out packet, if recieving two messages at once
	eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();

	if (&msg->target == NULL || rimeaddr_cmp(&msg->target, &rimeaddr_node_addr)) // Sink
	{
		// Start HSEND
		// TODO: pass arguments from trickle message to HSEND
		process_start(hsendProcess, &msg);
	}
}

static const trickle_callbacks callbacks = { &trickle_rcv };


PROCESS(mainProcess, "MAIN Process");
PROCESS(hsendProcess, "HSEND Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	trickle_open(&tc, trickle_interval, 121, &callbacks);

	if (!hsend_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t), &receieved_data))
	{
		printf("hsend start function failed\n");
	}

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
	hsend_end(&hc);
	trickle_close(&tc);
	PROCESS_END();
}


PROCESS_THREAD(hsendProcess, ev, msg)
{
	static hsend_conn_t hc;
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// 10 second timer
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	char const * bytecode = ((char const *)msg) + 
							sizeof(eval_pred_req_t) + 
							((&msg->num_of_bytecode_var * sizeof(unsigned char)) * 2) ;

	variables = malloc(sizeof(var_elem_t) * &msg->num_of_bytecode_var);

	//pointer for bytecode variables
	char const * ptr = ((char const *)msg) + sizeof(eval_pred_req_t); 
	int max_hops = 0;
	int i;
	for (i = 0; i < &msg->num_of_bytecode_var; i++)
	{
		//create temporary elements
		var_elem_t *tmp = malloc(sizeof(var_elem_t));
		
		//populate the struct
		&tmp->hops = &ptr[(2 * i)];
		&tmp->var_id = &ptr[(2 * i)+1];

		if (&tmp->hops > max_hops)
		{
			max_hops = &tmp->hops;
		}

		//insert into the array
		variables[i] = &tmp;
	}


	hops_data = malloc(sizeof(int) * max_hops);
	for (i = 0; i < max_hops; i++)
	{
		LIST_STRUCT(hops_data[i]);
	}

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
	PROCESS_END();
}