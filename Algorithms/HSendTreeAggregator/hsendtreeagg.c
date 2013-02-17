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
//#include "neighbour-detect.h"

#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"
#include "unique-array.h"
#include "array-list.h"
 
static const clock_time_t ROUND_LENGTH = 5 * 60 * CLOCK_SECOND;

// The neighbours the current node has
// This is a list of rimeaddr_t
static unique_array_t one_hop_neighbours;

static array_list_t data_list;

typedef struct
{
	uint8_t round_count;
	array_list_t list;
} aggregation_data_t;

typedef struct
{
	rimeaddr_t first;
	rimeaddr_t second;
} rimeaddr_pair_t;

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

static bool rimeaddr_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	return rimeaddr_cmp((rimeaddr_t const *)left, (rimeaddr_t const *)right);
}

PROCESS(data_gather, "Data Gather");
PROCESS(send_data_process, "Send data process");

AUTOSTART_PROCESSES(&data_gather);

//Sink recieved final set of data
static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);


	//extract data from packet buffer
	collected_data_t const * msg = (collected_data_t const *)packetbuf_dataptr();

	uint8_t length = msg->length;

	node_data_t * msgdata = (node_data_t *)(msg + 1); //get the pointer after the message

	unsigned int i;
	
	for(i = 0; i < length; ++i)
	{
		printf("Tree Agg: Recv, Node: %s Temp:%d, Humidity: %d\n", 
			addr2str(&msgdata[i].addr), 
			(int)msgdata[i].temp, 
			msgdata[i].humidity);
	}

	//evaluate
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
	toggle_led_for(LEDS_RED, CLOCK_SECOND);

	printf("Tree Agg: Update local data\n");

	unique_array_t * data = &((aggregation_data_t *)voiddata)->list;
	collected_data_t const * data_to_apply = (collected_data_t const *)to_apply;
	
	node_data_t * msgdata = (node_data_t *)(data_to_apply + 1); //get the pointer after the message

	unsigned int i;
	for(i = 0; i< data_to_apply->length; ++i)
	{
		node_data_t * tmp = (node_data_t *)malloc(sizeof(node_data_t));
		tmp->temp = msgdata[i].temp;
		tmp->humidity = msgdata[i].humidity;

		array_list_append(&data->list, tmp);
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

	array_list_append(&data,&msg);
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
	
	node_data_t * msgdata = (node_data_t *)(msg + 1); //get the pointer after the message

	unsigned int i;
	for(i = 0; i< msg->length; ++i)
	{
		node_data_t * tmp = (node_data_t *)malloc(sizeof(node_data_t));
		tmp->temp = msgdata[i].temp;
		tmp->humidity = msgdata[i].humidity;

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
	uint8_t length = array_list_length(&conn_data->list);
	unsigned int packet_length = sizeof(node_data_t) + sizeof(collected_data_t) * length;

	packetbuf_clear();
	packetbuf_set_datalen(packet_length);
	debug_packet_size(packet_length);

	collected_data_t * msg = (collected_data_t *)packetbuf_dataptr();
	msg->length = length;
	msg->round_count = conn_data->round_count;

	printf("Writing packet, length %d\n data length:%d", msg->length,length);

	node_data_t * msgdata = (node_data_t *)(msg + 1); //get the pointer after the message

	unsigned int i = 0;
	unique_array_elem_t elem;
	for (elem = array_list_first(&conn_data->list); 
		array_list_continue(&conn_data->list, elem);
		elem = array_list_next(elem))
	{
		node_data_t const * original = (node_data_t *)array_list_data(&conn_data->list, elem);

		rimeaddr_copy(&msgdata[i].addr, &original->addr);
		msgdata[i].temp = original->temp;
		msgdata[i].humidity = &original->humidity;

		++i;
	}

	// Free the data here
	array_list_clear(&conn_data->list);
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

	// Wait for some time to let process start up and perform neighbourgh detect
	array_list_init(&data_list,NULL);

	//start_neighbour_detect(&one_hop_neighbours, 150);
	
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	printf("Starting Tree Aggregation\n");
	tree_agg_open(&aggconn, &sink, 118, 132, sizeof(aggregation_data_t), &callbacks);

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

			//send data
			tree_agg_send(&aggconn);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		++round_count;
	}

exit:
	unique_array_clear(&one_hop_neighbours);
	tree_agg_close(&aggconn);
	PROCESS_END();
}