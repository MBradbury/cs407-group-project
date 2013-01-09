#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "dev/leds.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/unicast.h"
#include "contiki-net.h"

#include "sensor-converter.h"
#include "debug-helper.h"
#include "lib/list.h"

#include "tree-aggregator.h"
#include "neighbour-detect.h"

//PACKETBUF_SIZE

typedef struct
{
	rimeaddr_t addr1;
	rimeaddr_t addr2;
} list_elem_t

static void get_neighbours(collect_msg_t * data)
{

}

static void union_neighbours(list_t * data, rime_addr_t addr1, rime_addr_t addr2)
{
	list_elem_t * iterator = NULL;
	for (iterator = (list_elem_t *)list_head(&data);
		 iterator != NULL;
		 iterator = (list_elem_t *)list_item_next(iterator))
	{
		if ((rimeaddr_cmp(iterator->addr1,&addr1)!=0 && rimeaddr_cmp(iterator->addr2,&addr2)!=0)||
		(rimeaddr_cmp(iterator->addr1,&addr2)!=0 && rimeaddr_cmp(iterator->addr2,&addr1)!=0))
		{
			return
		}
	}
	list_elem_t * new_pair = (list_elem_t *)malloc(sizeof(list_elem_t))
	rimeaddr_copy(new_pair->addr1, &addr1);
	rimeaddr_copy(new_pair->addr2, &addr2);
	
	list_push(data, new_pair);
}

/********************************************
 ********* APPLICATION BEGINS HERE **********
 *******************************************/


typedef struct
{
	unsigned int length;	// Number of address pairs
} collect_msg_t;


PROCESS(startup_process, "Startup");
PROCESS(send_data_process, "Data Sender");

AUTOSTART_PROCESSES(&startup_process);


static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source)
{
	collect_msg_t const * msg = (collect_msg_t const *)packetbuf_dataptr();

	printf("Sink rcv: Src:%s Temp:%d Hudmid:%d%%\n",
			addr2str(source),
			(int)msg->temperature, (int)msg->humidity
	);
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	if (!is_sink(conn))
	{
		process_start(&send_data_process, (char *)conn);
	}
}

static void tree_aggregate_update(void * data, void const * to_apply)
{
	
	list_t * data_list = (list_t *)data;
	collect_msg_t const * data_to_apply = (collect_msg_t const *)to_apply;
	unsigned int const * cp = (unsigned int const * to_apply);
	unsigned int length = *cp;
	
	cp++;
	
	rimeaddr_t const * ap = (rimeaddr_t const * cp);
	for (unsigned int i = 0; i < length; i+=2)
	{
		union_neighbours(data_list, ap[i], ap[i + 1]);
	}
}

static void tree_aggregate_own(void * ptr)
{


	tree_aggregate_update(ptr, &data);
}

static tree_agg_conn_t conn;
static tree_agg_callbacks_t callbacks =
	{ &tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update, &tree_aggregate_own };

PROCESS_THREAD(startup_process, ev, data)
{
	static rimeaddr_t sink;

	PROCESS_BEGIN();

	sink.u8[0] = 1;
	sink.u8[1] = 0;

	tree_agg_open(&conn, &sink, 118, 132, sizeof(collect_msg_t), &callbacks);

	PROCESS_END();
}


PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	printf("Starting data generation process\n");

	leds_on(LEDS_GREEN);

	// By this point the tree should be set up,
	// so now we should move to aggregating data
	// through the tree

	etimer_set(&et, 60 * CLOCK_SECOND);
 
	// Only leaf nodes send these messages
	while (tree_agg_is_leaf(&conn))
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Read the data from the temp and humidity sensors
		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);


		// Create the data message that we are going to send
		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		debug_packet_size(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
		memset(msg, 0, sizeof(collect_msg_t));

		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);
		
		tree_agg_send(&conn);

		etimer_reset(&et);
	}

	PROCESS_END();
}

