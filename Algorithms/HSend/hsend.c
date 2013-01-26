#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "contiki.h"

#include "dev/leds.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"
#include "lib/sensors.h"
#include "net/rime.h"

#include "nhopreq.h"
#include "predlang.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "map.h"

// Struct for the list of node_data. It contains owner_addr, temperature and humidity. 
typedef struct
{
	rimeaddr_t addr;
	nfloat temp;
	nfloat humidity;
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
	rimeaddr_t target;
	uint8_t bytecode_length; //length of the bytecode_instructions, located after the struct
	uint8_t num_of_bytecode_var; //number of variables after the struct
} eval_pred_req_t;

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
	}
}
///
/// End VM Helper Functions
///

static map_t * hops_data = NULL;

// Count the number of elements added to each of the lists
static unsigned int max_size = 0;


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


	map_t * map = &hops_data[hops - 1];

	// Check that we have not previously received data from this node before
	node_data_t * stored = (node_data_t *)map_get(map, from);
	
	if (stored != NULL)
	{
		memcpy(stored, nd, sizeof(node_data_t));
	}
	else
	{
		stored = (node_data_t *)malloc(sizeof(node_data_t));
		memcpy(stored, nd, sizeof(node_data_t));

		map_put(map, stored);
		max_size++;
	}
}



static nhopreq_conn_t hc;
static struct trickle_conn tc;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;


PROCESS(hsendProcess, "HSEND Process");

static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};


// Rime address of target node (or rimeaddr_null for everyone)
// binary bytecode for the VM
static void trickle_rcv(struct trickle_conn * c)
{
	// Copy out packet, allows handling multiple evals at once
	eval_pred_req_t const * msg = (eval_pred_req_t *)packetbuf_dataptr();

	//printf("Rcv packet length %d\n", packetbuf_datalen());
		
	if (rimeaddr_cmp(&msg->target, &rimeaddr_null) ||		// Send to all
		rimeaddr_cmp(&msg->target, &rimeaddr_node_addr)) 	// We are the target
	{
		eval_pred_req_t * msgcopy = (eval_pred_req_t *)malloc(packetbuf_datalen());
		memcpy(msgcopy, msg, packetbuf_datalen());

		// Start HSEND
		printf("Got message, starting evaluation!\n");
		process_start(&hsendProcess, msgcopy);
	}
}

static const struct trickle_callbacks callbacks = { &trickle_rcv };

PROCESS(mainProcess, "MAIN Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;
	static rimeaddr_t destination;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	// Set the predicate evaluation target
	destination.u8[0] = 10;
	destination.u8[1] = 0;

	trickle_open(&tc, trickle_interval, 121, &callbacks);

	if (!nhopreq_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t), &receieved_data))
	{
		printf("nhopreq start function failed\n");
	}

	if (rimeaddr_cmp(&baseStationAddr, &rimeaddr_node_addr)) // Sink
	{
		printf("Is the base station!\n");

		// Send the request message
		
		
		uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
		uint8_t var_details = 2;
		
		unsigned int packet_size = sizeof(eval_pred_req_t) + bytecode_length + (sizeof(var_elem_t) * var_details);

		packetbuf_clear();
		packetbuf_set_datalen(packet_size);
		debug_packet_size(packet_size);
		eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();
		memset(msg, 0, packet_size);

		rimeaddr_copy(&msg->target, &destination);
		msg->bytecode_length = bytecode_length;
		msg->num_of_bytecode_var = var_details;

		var_elem_t * msg_vars = (var_elem_t *)(msg + 1);

		msg_vars[0].hops = 2;
		msg_vars[0].var_id = 255;
		msg_vars[1].hops = 1;
		msg_vars[1].var_id = 254;

		ubyte * msg_bytecode = (ubyte *)(msg_vars + var_details);

		if ((void *)(msg_bytecode + bytecode_length) - (void *)msg != packet_size)
		{
			printf("Failed to copy data correctly got=%d expected=%d!\n",
				(void *)(msg_bytecode + bytecode_length) - (void *)msg,
				packet_size);
		}

		memcpy(msg_bytecode, program_bytecode, bytecode_length);

		//printf("Sent packet length %d\n", packet_size);

		trickle_send(&tc);


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
	nhopreq_end(&hc);
	trickle_close(&tc);
	PROCESS_END();
}


static bool rimeaddr_equal_node_data(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	node_data_t const * l = (node_data_t const *)left;
	node_data_t const * r = (node_data_t const *)right;

	return rimeaddr_cmp(&l->addr, &r->addr);
}


PROCESS_THREAD(hsendProcess, ev, data)
{
	static struct etimer et;
	static eval_pred_req_t const * msg;
	static var_elem_t const * variables;
	static ubyte const * bytecode_instructions;
	static unsigned int max_hops = 0;

	static node_data_t * all_neighbour_data = NULL;
	static unsigned int count = 0;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	msg = (eval_pred_req_t const *)data;
	
	printf("HSEND Process Started\n");

	//Wait for other nodes to initialize.
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	printf("Wait finished\n");

	// Pointer for bytecode variables
	variables = (var_elem_t const *)(msg + 1);

	// Create a pointer to the bytecode instructions stored in the message.
	bytecode_instructions = (ubyte const *)(variables + msg->num_of_bytecode_var);

	unsigned int i;
	for (i = 0; i < msg->num_of_bytecode_var; i++)
	{
		if (variables[i].hops > max_hops)
		{
			max_hops = variables[i].hops;
		}

		//printf("variables added: %d %d\n",variables[i].hops,variables[i].var_id);
	}

	// Only ask for data if the predicate needs it
	if (max_hops != 0)
	{
		hops_data = (map_t *) malloc(sizeof(map_t) * max_hops);

		for (i = 0; i < max_hops; i++)
		{
			map_init(&hops_data[i], &rimeaddr_equal_node_data, &free);
		}
	
		printf("Starting request for %d hops of data...\n", max_hops);

		nhopreq_request_info(&hc, max_hops);
	
		// Get as much information as possible within a given time bound
		etimer_set(&et, 45 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf("Finished collecting hop data.\n");


		// Generate array of all the data
		all_neighbour_data = (node_data_t *) malloc(sizeof(node_data_t) * max_size);

		count = 0; // position in all_neighbour_data

		for (i = 0; i < max_hops ; i++)
		{
			unsigned int length = map_length(&hops_data[i]);

			if (length > 0)
			{
				map_elem_t elem;
				for (elem = map_first(&hops_data[i]); 
					map_continue(&hops_data[i], elem); 
					elem = map_next(elem))
				{
					node_data_t * mapdata = (node_data_t *)map_data(&hops_data[i], elem);
					memcpy(&all_neighbour_data[count], mapdata, sizeof(node_data_t));
					count++;
				}
			}

			printf("i=%d Count=%d length=%d\n", i, count, length);
		}
	}

	printf("Starting predicate evaluation with code length: %d.\n", msg->bytecode_length);

	// Set up the predicate language VM
	init_pred_lang(&node_data, sizeof(node_data_t));

	// Register the data functions 
	register_function(0, &get_addr, TYPE_INTEGER);
	register_function(2, &get_temp, TYPE_FLOATING);
	register_function(3, &get_humidity, TYPE_FLOATING);


	printf("Binding variables using %p\n", all_neighbour_data);

	// Bind the variables to the VM
	for (i = 0; i < msg->num_of_bytecode_var; ++i)
	{
		// Get the length of this hop's data
		// including all of the closer hop's data length
		unsigned int length = 0;
		unsigned int j;
		for (j = 0; j < variables[i].hops; ++j)
		{
			length += map_length(&hops_data[j]);
		}

		printf("Binding variables: var_id=%d hop=%d length=%d\n", variables[i].var_id, variables[i].hops, length);
		bind_input(variables[i].var_id, all_neighbour_data, length);
	}

	nbool evaluation = evaluate(bytecode_instructions, msg->bytecode_length);

	// TODO: If predicate failed inform sink
	// TODO: Send data back to sink
	if (evaluation)
	{
		printf("Pred: TRUE\n");
	}
	else
	{
		printf("Pred: FAILED due to error: %s\n", error_message());
	}

	free(msg);
	
	// Free all the lists
	for (i = 0; i < max_hops; i++)
	{
		map_clear(&hops_data[i]);
	}

	free(all_neighbour_data);
	free(hops_data);
	free(variables);

exit:
	printf("Exiting HSEND Process...\n");
	PROCESS_END();
}

