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
#include "linked-list.h"

#define ID_FN_ID 0
#define SLOT_FN_ID 1
#define TEMP_FN_ID 2
#define HUMIDITY_FN_ID 3

// Struct for the list of node_data. It contains owner_addr, temperature and humidity. 
typedef struct
{
	rimeaddr_t addr;
	nfloat temp;
	nfloat humidity;
} node_data_t;

// Struct for the list of bytecode_variables. It contains the variable_id and hop count.
typedef struct var_elem
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

//VM Accessor functions.
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

var_elem_t * variables = NULL; //array of the variables from bytecode
linked_list_t * hops_data = NULL;
int max_size = 0; //Count the number of elements added to the list

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

static void receieved_data(rimeaddr_t const * from, uint8_t hops, node_data_t const * data)
{
	node_data_t * nd = malloc(sizeof(node_data_t));
	nd->humidity = data->humidity;
	nd->temp = data->temp;
	rimeaddr_copy(&nd->addr,&data->addr);

	char from_str[RIMEADDR_STRING_LENGTH];
	char addr_str[RIMEADDR_STRING_LENGTH];

	printf("Obtained information from %s (%s) hops:%u, T:%d H:%d%%\n",
		addr2str_r(from, from_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&nd->addr, addr_str, RIMEADDR_STRING_LENGTH),
		hops,
		(int)nd->temp, (int)nd->humidity);

	linked_list_append(&hops_data[hops], nd);
	max_size++;
}



static hsend_conn_t hc;
static struct trickle_conn tc;
static rimeaddr_t baseStationAddr;

static const clock_time_t trickle_interval = 2 * CLOCK_SECOND;


PROCESS(hsendProcess, "HSEND Process");

//Rime adress of target node (or null for everyone)
//binary bytecode for the VM
static void trickle_rcv(struct trickle_conn * c)
{
	//TODO: might have to copy out packet, if recieving two messages at once
	eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();

	if (&msg->target == NULL || rimeaddr_cmp(&msg->target, &rimeaddr_node_addr)) // Sink
	{
		// Start HSEND
		process_start(&hsendProcess, &msg);
	}
}

static const struct trickle_callbacks callbacks = {trickle_rcv};

PROCESS(mainProcess, "MAIN Process");

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
		eval_pred_req_t *msg = malloc(sizeof(eval_pred_req_t));
		msg->bytecode_length = 3;
		msg->num_of_bytecode_var = 0;
		rimeaddr_copy(&msg->target,&rimeaddr_node_addr);

		process_start(&hsendProcess, msg);

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


PROCESS_THREAD(hsendProcess, ev, d)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	printf("HSEND Process Stared\n");

	//Wait for other nodes to initialize.
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	eval_pred_req_t * msg = (eval_pred_req_t *)d; //type cast the data

	//Create a pointer to the bytecode instructions stored in the message.
	char const * bytecode_instructions = ((char const *)msg) + 
							sizeof(eval_pred_req_t) + 
							((msg->num_of_bytecode_var * sizeof(unsigned char)) * 2);

	//Create an array to store the bytecode variables in.
	variables = (var_elem_t *) malloc(sizeof(var_elem_t) * msg->num_of_bytecode_var);

	//pointer for bytecode variables
	char const * ptr = ((char const *)msg) + sizeof(eval_pred_req_t); 
	uint8_t max_hops = 0;

	int i;
	for (i = 0; i < msg->num_of_bytecode_var; i++)
	{
		//create temporary elements
		var_elem_t * tmp = (var_elem_t *) malloc(sizeof(var_elem_t));
		
		//populate the struct
		tmp->hops = ptr[(2 * i)];
		tmp->var_id = ptr[(2 * i)+1];

		if (tmp->hops > max_hops)
		{
			max_hops = tmp->hops;
		}

		//insert into the array
		variables[i] = *tmp;
	}

	hops_data = (linked_list_t *) malloc(1 + (sizeof(linked_list_t) * max_hops));

	for (i = 0; i < max_hops + 1; i++)
	{
		linked_list_init(&hops_data[i], NULL);
	}


	printf("Sending pred req\n");

	if (max_hops != 0)
	{
		hsend_request_info(&hc, max_hops);
	
		// Get as much information as possible within a given time bound
		etimer_set(&et, 60 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	//Generate array of all the data
	node_data_t * vm_hop_data = (node_data_t * )malloc(1 + (sizeof(node_data_t) * max_size));

	int * locations = malloc(1 + (sizeof(int) * max_hops)); 
	int count = 0; //position in vm_hop_data

	//TODO: get actual values
	//Put self into the list, at 0 hops
	node_data_t * self = malloc(sizeof(node_data_t));
	self->temp = 10.0;
	self->humidity = 10.0;
	rimeaddr_copy(&self->addr, &rimeaddr_node_addr);

	linked_list_append(&hops_data[0], self);
	printf("Append\n");
	max_size++;

	for (i = 0; i < max_hops + 1; i++)
	{
		linked_list_elem_t * elem;
		for (elem = linked_list_first(&hops_data[i]); 
			linked_list_continue(&hops_data[i], elem); 
			elem = linked_list_next(elem))
		{
			memcpy(&vm_hop_data[count], linked_list_data(&hops_data[i], elem), sizeof(node_data_t));
			count++;
		}

		locations[i] = count - 1;

		linked_list_clear(&hops_data[i]);
	}
		printf("Hello\n");


	// Set up the predicate language VM
	init_pred_lang(&vm_hop_data, sizeof(node_data_t));

	// Register the data functions 
	register_function(0, &get_addr, TYPE_INTEGER);
	register_function(1, &get_temp, TYPE_FLOATING);
	register_function(2, &get_humidity, TYPE_FLOATING);

	//Bind the variables to the VM
	for (i = 0; i < msg->num_of_bytecode_var; ++i)
	{
		bind_input(variables[i].var_id, &vm_hop_data, locations[variables[i].hops]-1);
	}
	

	ubyte code[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};

	nbool evaluation = evaluate(code, msg->bytecode_length);

	// TODO: If predicate failed inform sink
	// TODO: Send data back to sink
	if (evaluation != 0)
	{
		printf("%s\n","Pred: TRUE" );
	}
	else
	{
		printf("%s\n","failed");
	}

	free(self);
	free(locations);

exit:
	printf("Exiting HSEND Process...\n");
	PROCESS_END();
}
