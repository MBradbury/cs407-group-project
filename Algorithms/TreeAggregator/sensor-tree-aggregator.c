#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "dev/sht11-sensor.h"
#include "dev/light-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "contiki-net.h"

#include "lib/random.h"

#include "random-range.h"
#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"

#include "net/tree-aggregator.h"


typedef struct
{
	double temperature;
	double humidity;
	double light1;
	double light2;
} collect_msg_t;


PROCESS(startup_process, "Startup");
PROCESS(send_data_process, "Data Sender");

AUTOSTART_PROCESSES(&startup_process);


static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source, void const * packet, unsigned int length)
{
	collect_msg_t const * msg = (collect_msg_t const *)packet;

	printf("Tree Agg: Sink rcv: Src:%s Temp:%d Hudmid:%d%% Light1:%d Light2:%d\n",
			addr2str(source),
			(int)msg->temperature, (int)msg->humidity, (int)msg->light1, (int)msg->light2
	);
}

static void tree_agg_setup_finished(tree_agg_conn_t * conn)
{
	if (!tree_agg_is_sink(conn))
	{
		process_start(&send_data_process, (char *)conn);
	}
}

static void tree_aggregate_update(void * data, void const * to_apply, unsigned int length)
{
	collect_msg_t * our_data = (collect_msg_t *)data;
	collect_msg_t const * data_to_apply = (collect_msg_t const *)to_apply;

	our_data->temperature += data_to_apply->temperature;
	our_data->humidity += data_to_apply->humidity;

	our_data->temperature /= 2.0;
	our_data->humidity /= 2.0;

	our_data->light1 += data_to_apply->light1;
	our_data->light2 += data_to_apply->light2;

	our_data->light1 /= 2.0;
	our_data->light2 /= 2.0;
}

static void tree_aggregate_own(void * ptr)
{
	collect_msg_t data;

	SENSORS_ACTIVATE(sht11_sensor);
	int raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
	int raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	SENSORS_DEACTIVATE(sht11_sensor);

	data.temperature = sht11_temperature(raw_temperature);
	data.humidity = sht11_relative_humidity_compensated(raw_humidity, data.temperature);

	SENSORS_ACTIVATE(light_sensor);
	int raw_light1 = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
	int raw_light2 = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);
	SENSORS_DEACTIVATE(light_sensor);

	data.light1 = s1087_light1(raw_light1);
	data.light2 = s1087_light1(raw_light2);

	tree_aggregate_update(ptr, &data, 1);
}

static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	memcpy(conn->data, packet, length);
}

static void tree_agg_write_data_to_packet(tree_agg_conn_t * conn, void ** data, size_t * length)
{
	*length = conn->data_length;
	*data = malloc(*length);
	memcpy(*data, conn->data, conn->data_length);
}

static tree_agg_conn_t conn;
static tree_agg_callbacks_t callbacks = {
	&tree_agg_recv, &tree_agg_setup_finished, &tree_aggregate_update,
	&tree_aggregate_own, &tree_agg_store_packet, &tree_agg_write_data_to_packet
};

PROCESS_THREAD(startup_process, ev, data)
{
	static rimeaddr_t sink;

	PROCESS_BEGIN();

#ifdef POWER_LEVEL
	cc2420_set_txpower(POWER_LEVEL);
#endif

	sink.u8[0] = 1;
	sink.u8[1] = 0;

	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	tree_agg_open(&conn, &sink, 118, 132, sizeof(collect_msg_t), &callbacks);

	PROCESS_END();
}


PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	printf("Tree Agg: Starting data generation process\n");

	leds_on(LEDS_GREEN);

	// By this point the tree should be set up,
	// so now we should move to aggregating data
	// through the tree

	etimer_set(&et, 60 * CLOCK_SECOND);
 
	// Only leaf nodes send these messages
	while (tree_agg_is_leaf(&conn))
	{
		leds_on(LEDS_BLUE);

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Read the data from the temp and humidity sensors
		SENSORS_ACTIVATE(sht11_sensor);
		int raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		int raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		SENSORS_ACTIVATE(light_sensor);
		int raw_light1 = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
		int raw_light2 = light_sensor.value(LIGHT_SENSOR_TOTAL_SOLAR);
		SENSORS_DEACTIVATE(light_sensor);

		collect_msg_t * msg = (collect_msg_t *)malloc(sizeof(collect_msg_t));

		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);
		msg->light1 = s1087_light1(raw_light1);
		msg->light2 = s1087_light1(raw_light2);
		
		tree_agg_send(&conn, msg, sizeof(collect_msg_t));

		etimer_reset(&et);

		leds_off(LEDS_BLUE);
	}

	PROCESS_END();
}


