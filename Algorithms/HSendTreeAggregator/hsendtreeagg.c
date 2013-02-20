#include "contiki.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "contiki-net.h"

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

#include "tree-aggregator.h"
#include "neighbour-aggregate.h"

#include "predlang.h"

#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "array-list.h"
#include "unique-array.h"
#include "map.h"


static const clock_time_t ROUND_LENGTH = 10 * 60 * CLOCK_SECOND;

static map_t neighbour_info;

static map_t recieved_data;

static array_list_t predicates;

// An array_list_t of map_t of node_data_t
static array_list_t hops_data;

// Struct for the list of bytecode_variables. It contains the variable_id and hop count.
typedef struct
{
	uint8_t hops;
	uint8_t var_id;
} var_elem_t;

typedef struct
{
	rimeaddr_t destination; //where the predicate should be evaluated from
	uint8_t id; // Keep id as the first variable in the struct
	uint8_t variables_details_length;
	uint8_t bytecode_length;

	var_elem_t * variables_details;
	ubyte * bytecode;

} predicate_detail_entry_t;

typedef struct
{
	uint8_t round_count;
	array_list_t list;
} aggregation_data_t;

typedef struct
{
    uint8_t length;
    uint8_t round_count;
} collected_data_t;

// Struct for the list of node_data. It contains owner_addr, round count, temperature and humidity. 
typedef struct
{
	rimeaddr_t addr;
	float temp;
	int humidity;
	//nint light1;
	//nint light2;
} node_data_t;

typedef struct
{
	int key;
	unique_array_t * data;
} neighbour_map_elem_t; 

typedef struct
{
	int key;
	map_t * data;
} node_data_map_elem_t; 

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

//TODO: Convert this to work with a target node (i.e. use the data from another node)
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

static void predicate_detail_entry_cleanup(void * item)
{
	predicate_detail_entry_t * entry = (predicate_detail_entry_t *)item;

	free(entry->variables_details);
	free(entry->bytecode);
	free(entry);
}

static bool intCompare(void const * x, void const * y)
{
	neighbour_map_elem_t const * a = x;
	neighbour_map_elem_t const * b = y;

	return (a->key < b->key) ? -1 : (a->key > b->key);
}

static bool rimeaddr_pair_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	rimeaddr_pair_t const * lp = (rimeaddr_pair_t const *)left;
	rimeaddr_pair_t const * rp = (rimeaddr_pair_t const *)right;

	return
		(rimeaddr_cmp(&lp->first, &rp->first) && rimeaddr_cmp(&lp->second, &rp->second)) ||
		(rimeaddr_cmp(&lp->second, &rp->first) && rimeaddr_cmp(&lp->first, &rp->second));
}

static bool node_data_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	node_data_t const * l = (node_data_t const *)left;
	node_data_t const * r = (node_data_t const *)right;

	return rimeaddr_cmp(&l->addr, &r->addr);
}

static bool rimeaddr_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	return rimeaddr_cmp((rimeaddr_t const *)left, (rimeaddr_t const *)right);
}

//TODO: add clearing of previous neighbour information
/* to be called when neighbour aggregate gets some data to add */
static void handle_neighbour_data(rimeaddr_pair_t * pairs, unsigned int length, int round_count)
{
	printf("Handling neighbour data\n");
	//use a map based on round_count, map contains a unique array list of all the neighbour pairs

	//check if round is in map already, if not create new unique array list
	int * r = (int *)malloc(sizeof(int));
	r = &round_count;
	printf("Fetching information\n");

	neighbour_map_elem_t *stored = (unique_array_t *)map_get(&neighbour_info, &r);
	unique_array_t * information;

	if(stored) //saved before
	{
		information = &stored->data;
	}
	else
	{
		unique_array_init(information, &rimeaddr_pair_equality, &free);
		
		neighbour_map_elem_t * elem = (neighbour_map_elem_t *)malloc(sizeof(neighbour_map_elem_t));
		elem->key = round_count;
		elem->data = information;

		map_put(&neighbour_info, elem); 
	}

	printf("got information\n");
	free(r);


	printf("begining adding pairs to the unique array\n");
	unsigned int i;
	for (i = 0; i < length; ++i)
	{
		rimeaddr_pair_t * p = (rimeaddr_pair_t *)malloc(sizeof(rimeaddr_pair_t));
		printf("created pair information:%d\n",information);

		rimeaddr_copy(&p->first, &pairs[i].first);
		rimeaddr_copy(&p->second, &pairs[i].second);
		printf("information size: %d\n",unique_array_length(information));

		printf("Copied in pairs to p\n");

		//printf("attempting to append to information %d\n",information);
		unique_array_append(information, &p); //add the pair to the list
	}
}

/* Gets the neighbours of a given node */
static unique_array_t * get_neighbours(rimeaddr_t const * target, int round_count)
{
	unique_array_t * output;
	unique_array_init(output, &rimeaddr_equality, &free);
	int * r = (int *)malloc(sizeof(int));
	r = &round_count;

	//pairs of neighbours for a given round
	unique_array_t * pairs = (unique_array_t *)map_get(&neighbour_info, &r);
	free(r);
	//go through each pair
	unique_array_elem_t elem;
	for (elem = unique_array_first(pairs); 
		unique_array_continue(pairs, elem); 
		elem = unique_array_next(elem))
	{
		rimeaddr_pair_t * data = (rimeaddr_pair_t *)unique_array_data(pairs, elem);

		rimeaddr_t first = data->first;
		rimeaddr_t second = data->second; 

		//if either match, add the other to the list
		if (rimeaddr_cmp(&first, target))
		{
			unique_array_append(output, &second);
		}
		else if (rimeaddr_cmp(&second, target))
		{
			unique_array_append(output, &first);
		}
	}

	return output;
}

PROCESS(data_gather, "Data Gather");
PROCESS(send_data_process, "Send data process");
PROCESS(data_evaluation_process, "Data evaluation process");

AUTOSTART_PROCESSES(&data_gather);

//Sink recieved final set of data
static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	//extract data from packet buffer
	collected_data_t const * msg = (collected_data_t const *)packetbuf_dataptr();

	uint8_t length = msg->length;

	node_data_t const * msgdata = (node_data_t const *)(msg + 1); //get the pointer after the message

	node_data_map_elem_t * st = (node_data_map_elem_t *)map_get(&recieved_data, (int *)msg->round_count); //map for that round

	map_t * round_data = st->data;

	if(round_data == NULL) //if the map hasn't been initialised
	{
		//init the new map
		map_init(round_data, &node_data_equality, &free);
		
		node_data_map_elem_t * elem = (node_data_map_elem_t *)malloc(sizeof(node_data_map_elem_t));
		elem->key = msg->round_count;
		elem->data = round_data;

		//add it to the main map
		map_put(&recieved_data, elem);
	}

	unsigned int i;
	for(i = 0; i < length; ++i)
	{
		printf("Tree Agg: Recv, Node: %s Temp:%d, Humidity: %d\n", 
			addr2str(&msgdata[i].addr), 
			(int)msgdata[i].temp, 
			msgdata[i].humidity);

		node_data_t * nd = (node_data_t *)malloc(sizeof(node_data_t));
		rimeaddr_copy(&nd->addr,&msgdata[i].addr);
		nd->temp = (int)msgdata[i].temp;
		nd->humidity = msgdata[i].humidity;

		//add the data to the map
		map_put(round_data, &nd);
	}
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	printf("Tree Agg: Setup finsihed\n");

	if (tree_agg_is_leaf(conn))
	{
		printf("Tree Agg: Is leaf starting data aggregation\n");

		leds_on(LEDS_RED);

		process_start(&send_data_process, NULL);
	}
}

static void tree_aggregate_update(void * voiddata, void const * to_apply)
{
	printf("Tree Agg: Update local data\n");

	toggle_led_for(LEDS_RED, CLOCK_SECOND);

	array_list_t * data = &((aggregation_data_t *)voiddata)->list;
	collected_data_t const * data_to_apply = (collected_data_t const *)to_apply;

	node_data_t const * msgdata = (node_data_t const *)(data_to_apply + 1); //get the pointer after the message

	unsigned int i;
	for(i = 0; i< data_to_apply->length; ++i)
	{
		node_data_t * tmp = (node_data_t *)malloc(sizeof(node_data_t));
		tmp->temp = msgdata[i].temp;
		tmp->humidity = msgdata[i].humidity;

		rimeaddr_copy(&tmp->addr,&msgdata[i].addr);

		array_list_append(data, tmp);
	}
}

//TODO: Add our own one hop data to the list
static void tree_aggregate_own(void * ptr)
{
	printf("Tree Agg: Update local data with own data\n");

	array_list_t * data =  &((aggregation_data_t *)ptr)->list;
	node_data_t * msg = (node_data_t *)malloc(sizeof(node_data_t));

	SENSORS_ACTIVATE(sht11_sensor);
	int raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
	int raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	SENSORS_DEACTIVATE(sht11_sensor);

	msg->temp = sht11_temperature(raw_temperature);
	msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temp);	
	rimeaddr_copy(&msg->addr,&rimeaddr_node_addr);//copy in the rime address

	array_list_append(data, msg);
}

//store an inbound packet to the datastructure
//Arguments are: Connection, Packet, packet length
static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	printf("Tree Agg: Store Packet\n");

	collected_data_t const * msg = (collected_data_t const *)packet; //get the packet as a struct

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;

	conn_data->round_count = msg->round_count;

	array_list_init(&conn_data->list, &free);
	
	node_data_t const * msgdata = (node_data_t const *)(msg + 1); //get the pointer after the message
	
	unsigned int i;
	for(i = 0; i< msg->length; ++i)
	{
		node_data_t * tmp = (node_data_t *)malloc(sizeof(node_data_t));
		tmp->temp = msgdata[i].temp;
		tmp->humidity = msgdata[i].humidity;

		rimeaddr_copy(&tmp->addr,&msgdata[i].addr);

		array_list_append(&conn_data->list, tmp);
	}
}

//write the data structure to the outbout packet buffer
static void tree_agg_write_data_to_packet(tree_agg_conn_t * conn)
{
	//take all data, write a struct to the buffer at the start, 
	//containing the length of the packet (as the number of node_data_t)
	//write the each one to memory
	toggle_led_for(LEDS_BLUE, CLOCK_SECOND);

	aggregation_data_t * conn_data = (aggregation_data_t *)conn->data;
	unsigned int length = array_list_length(&conn_data->list);
	unsigned int packet_length = sizeof(collected_data_t) + sizeof(node_data_t) * length;

	packetbuf_clear();
	packetbuf_set_datalen(packet_length);
	debug_packet_size(packet_length);

	collected_data_t * msg = (collected_data_t *)packetbuf_dataptr();
	msg->length = (uint8_t)length;
	msg->round_count = conn_data->round_count;

	printf("Writing packet, length %d data length:%d\n", msg->length,length);

	node_data_t * msgdata = (node_data_t *)(msg + 1); //get the pointer after the message

	unsigned int i = 0;
	array_list_elem_t elem;
	for (elem = array_list_first(&conn_data->list); 
		array_list_continue(&conn_data->list, elem);
		elem = array_list_next(elem))
	{
		node_data_t * original = (node_data_t *)array_list_data(&conn_data->list, elem);

		rimeaddr_copy(&msgdata[i].addr, &original->addr);
		msgdata[i].temp = original->temp;
		msgdata[i].humidity = original->humidity;

		++i;
	}

	// Free the data here
	array_list_clear(&conn_data->list);
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

static tree_agg_conn_t aggconn;
static const tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};

PROCESS_THREAD(data_gather, ev, data)
{
	static rimeaddr_t sink;
	static struct etimer et;

	PROCESS_BEGIN();

#ifdef NODE_ID
	node_id_burn(NODE_ID); //Burn the Node ID to the device
#endif

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL); //Set the power levels for the radio
#endif 

	//Assign the sink node, default as 1.0
	sink.u8[0] = 1;
	sink.u8[1] = 0;

	if (rimeaddr_cmp(&rimeaddr_node_addr,&sink))
	{
		printf("We are sink node.\n");
	}

	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	// Wait for some time to let process start up and perform neighbour detect
	
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));


	map_init(&neighbour_info, &intCompare, &free); //setup the map
	start_neighbour_aggregate(&handle_neighbour_data);

	printf("Starting Tree Aggregation\n");
	tree_agg_open(&aggconn, &sink, 150, 100, sizeof(aggregation_data_t), &callbacks);

	//if sink start the evaluation process to run in the background
	if(rimeaddr_cmp(&rimeaddr_node_addr, &sink))
	{
		//create and save example predicates
		array_list_init(&predicates, &free);
	
		map_init(&recieved_data, &intCompare, &free);

		static ubyte const program_bytecode[] = {0x30,0x01,0x01,0x01,0x00,0x01,0x00,0x00,0x06,0x01,0x0a,0xff,0x1c,0x13,0x31,0x30,0x02,0x01,0x00,0x00,0x01,0x00,0x00,0x06,0x02,0x0a,0xff,0x1c,0x13,0x2c,0x37,0x01,0xff,0x00,0x37,0x02,0xff,0x00,0x1b,0x2d,0x35,0x02,0x12,0x19,0x2c,0x35,0x01,0x12,0x0a,0x00};
		
		//create the predicate
		uint8_t bytecode_length = sizeof(program_bytecode)/sizeof(program_bytecode[0]);
		uint8_t var_details = 2;
		rimeaddr_t dest;
		dest.u8[0] = 10;
		dest.u8[1] = 0;
		predicate_detail_entry_t *pred = (predicate_detail_entry_t *)malloc(sizeof(predicate_detail_entry_t));
		rimeaddr_copy(&pred->destination, &dest);
		pred->id = 1;
		pred->bytecode_length = bytecode_length;
		pred->variables_details_length = var_details;
		
		var_elem_t * msg_vars = (var_elem_t *)(malloc(sizeof(var_elem_t) * var_details));

		msg_vars[0].hops = 2;
		msg_vars[0].var_id = 255;
		msg_vars[1].hops = 1;
		msg_vars[1].var_id = 254;

		pred->variables_details = msg_vars;
		pred->bytecode = program_bytecode;

		//add it to the list
		array_list_append(&predicates, &pred);

		//start the evauluation process
		process_start(&data_evaluation_process,NULL);
	}

	PROCESS_END();
}

PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;
	static uint8_t round_count;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	
	round_count = 0;

	while (true)
	{
		etimer_set(&et, ROUND_LENGTH);

		if (tree_agg_is_leaf(&aggconn))
		{
			//Tree should be set up by now
			//Start sending data up the tree

			//create data message
			packetbuf_clear();
			//set length of the buffer
			packetbuf_set_datalen(sizeof(collected_data_t) + sizeof(node_data_t));
			debug_packet_size(sizeof(collected_data_t) + sizeof(node_data_t));
			collected_data_t * msg = (collected_data_t *)packetbuf_dataptr();

			//copy in data into the buffer
			memset(msg, 0, sizeof(collected_data_t) + sizeof(node_data_t));

			msg->round_count = round_count;
			msg->length = 1;

			SENSORS_ACTIVATE(sht11_sensor);
			int raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
			int raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
			SENSORS_DEACTIVATE(sht11_sensor);
			
			node_data_t * msgdata = (node_data_t *)(msg + 1); //get the pointer after the message
			
			msgdata[0].temp = sht11_temperature(raw_temperature);
			msgdata[0].humidity = sht11_relative_humidity_compensated(raw_humidity, msgdata[0].temp);
			rimeaddr_copy(&msgdata[0].addr, &rimeaddr_node_addr);

			//send data
			tree_agg_send(&aggconn);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		++round_count;
	}

exit:
	tree_agg_close(&aggconn);
	PROCESS_END();
}

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
			map_init(map, &node_data_equality, &free);

			array_list_append(&hops_data, map);
		}
	}

	return (map_t *)array_list_data(&hops_data, hop - 1);
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

static void free_hops_data(void * voiddata)
{
	map_t * data = (map_t *)voiddata;
	map_clear(data);
	free(data);
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

	printf("Binding variables using %p\n", all_neighbour_data);

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


PROCESS_THREAD(data_evaluation_process, ev, data)
{
	static struct etimer et; 
	//use a separate round_count for this process, so there isn't any interference with the other processes
	static uint8_t round_count;
	static node_data_t * all_neighbour_data = NULL;

	PROCESS_BEGIN();

	round_count = 0;

	while(true)
	{
		etimer_set(&et, ROUND_LENGTH*1.2); //wait a little longer than the round length before evaluation
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		//go through the list of predicates
		array_list_elem_t elem;
		for (elem = array_list_first(&predicates); array_list_continue(&predicates, elem); elem = array_list_next(elem))
		{
		    predicate_detail_entry_t * pred = (predicate_detail_entry_t *)array_list_data(&predicates, elem);

		    rimeaddr_t destination = pred->destination; //target node

		    //work out the max number of hops needed for the predicate
		    unsigned int max_hops = maximum_hop_data_request(pred->variables_details, pred->variables_details_length);


			array_list_init(&hops_data, &free_hops_data);
			unsigned int max_size = 0;
		    //array of nodes that have been seen so far
		    unique_array_t seen_nodes;
		    unique_array_init(&seen_nodes, &rimeaddr_equality, &free);
		    unique_array_append(&seen_nodes, &destination); 

		    //array of nodes that we need the neighbours for
		    unique_array_t target_nodes;
		    unique_array_init(&target_nodes, &rimeaddr_equality, &free);
		    unique_array_append(&target_nodes, &destination); 

		    //Get the data for each hop level
			unsigned int hops;
			for (hops = 0; hops < max_hops; ++hops)
			{
				//array of nodes that we acquire during this round
			    unique_array_t acquired_nodes;
			    unique_array_init(&acquired_nodes, &rimeaddr_equality, &free);
			    unique_array_append(&acquired_nodes, &destination); 

				//for each node in the target nodes, get the immediate neighbours,
				unique_array_elem_t target;
				for (target = unique_array_first(&target_nodes); 
					unique_array_continue(&target_nodes, target); 
					target = unique_array_next(target))
				{
					rimeaddr_t const * t = unique_array_data(&target_nodes, target);
					unique_array_t * neighbours = get_neighbours(t, round_count); //get the neighbours of the node
					
					//go through the neighbours for the node
					unique_array_elem_t neighbour;
					for (neighbour = unique_array_first(neighbours); 
						unique_array_continue(neighbours, neighbour); 
						neighbour = unique_array_next(neighbour))
					{
						//the neighbour found
						rimeaddr_t const * n = unique_array_data(neighbours, neighbour);

						//if the neighbour hasn't been seen before
						if(!unique_array_contains(&seen_nodes,&n))
						{
							int * r = (int *)malloc(sizeof(int));
							r = round_count;
							//get the data
							node_data_map_elem_t * st = (node_data_map_elem_t *)map_get(&recieved_data, r); //map for that round
							free(r);
							map_t * round_data = st->data;

							node_data_t * nd = (node_data_t *)map_get(round_data, &n);
							

							//add the node to the target nodes for the next round
							unique_array_append(&acquired_nodes, &n);

							map_t * map = get_hop_map(hops);

							// Check that we have not previously received data from this node before
							node_data_t * stored = (node_data_t *)map_get(map, n);
							
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
					}
				}
				//merge the old target nodes to the seen nodes, set the new target nodes to the acquired nodes
				unique_array_merge(&seen_nodes,&target_nodes);
				unique_array_clear(&target_nodes);
				target_nodes = acquired_nodes;

			}

			//clear the unused arrays
			unique_array_clear(&seen_nodes);
			unique_array_clear(&target_nodes);

			//then run the evaluation using the collected data 

			// Generate array of all the data
			all_neighbour_data = (node_data_t *) malloc(sizeof(node_data_t) * max_size);

			uint8_t i;
			for (i = 1; i <= max_hops; ++i)
			{
				map_t * hop_map = get_hop_map(i);

				unsigned int length = map_length(hop_map);

				// Position in all_neighbour_data
				unsigned int count = 0;

				if (length > 0)
				{
					for (elem = map_first(hop_map); map_continue(hop_map, elem); elem = map_next(elem))
					{
						node_data_t * mapdata = (node_data_t *)map_data(hop_map, elem);
						memcpy(&all_neighbour_data[count], mapdata, sizeof(node_data_t));
						++count;
					}
				}

				printf("i=%d Count=%d length=%d\n", i, count, length);
			}


			bool evaluation_result = evaluate_predicate(
			pred->bytecode, pred->bytecode_length,
			all_neighbour_data,
			pred->variables_details, pred->variables_details_length);

			if (evaluation_result)
			{
				printf("Pred: TRUE\n");
			}
			else
			{
				printf("Pred: FAILED due to error: %s\n", error_message());
			}


			array_list_clear(&hops_data);
		} //end of predicate loop

		++round_count; //increase the round count
	}
	PROCESS_END();
}
