/**
 * This file was mostly lifted from 
 * contiki-2.6/examples/sky/sky-collect.c
 */

#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/broadcast.h"
#include "net/rime/timesynch.h"
#include "contiki-net.h"

double sht11_relative_humidity(unsigned raw)
{
	// FROM:
	// http://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/Humidity/Sensirion_Humidity_SHT1x_Datasheet_V5.pdf
	// Page 8

	// 12-bit
	static const double c1 = -2.0468;
	static const double c2 = 0.0367;
	static const double c3 = -1.5955E-6;

	// 8-bit
	/*static const double c1 = -2.0468;
	static const double c2 = 0.5872;
	static const double c3 = -4.0845E-4;*/

	return c1 + c2 * raw + c3 * raw * raw;
}

double sht11_relative_humidity_compensated(unsigned raw, double temperature)
{
	// 12-bit
	static const double t1 = 0.01;
	static const double t2 = 0.00008;

	// 8-bit
	/*static const double t1 = 0.01;
	static const double t2 = 0.00128;*/

	double humidity = sht11_relative_humidity(raw);

	return (temperature - 25) * (t1 + t2 * raw) + humidity;
}

/** Output temperature in degrees Celcius */
double sht11_temperature(unsigned raw)
{
	//static const double d1 = -40.1; // 5V
	//static const double d1 = -39.8; // 4V
	//static const double d1 = -39.7; // 3.5V
	static const double d1 = -39.6; // 3V
	//static const double d1 = -39.4; // 2.5V

	static const double d2 = 0.01; // 14-bit
	//static const double d2 = 0.04; // 12-bit

	return d1 + d2 * raw;
}


static bool is_sink()
{
	return rimeaddr_node_addr.u8[0] == 1 &&
		rimeaddr_node_addr.u8[1] == 0;
}

typedef enum
{
	collect_message_type,
	setup_message_type
} message_type_t;

typedef struct
{
	message_type_t type;
} base_msg_t;

/** The structure of the message we are sending */
typedef struct
{
	base_msg_t base;

	rimeaddr_t destination;

	double temperature;
	double humidity;
} collect_msg_t;

typedef struct
{
	base_msg_t base;

	rimeaddr_t id;
	rimeaddr_t parent;
	uint32_t hop_count;

} setup_tree_msg_t;

static struct ctimer ct;

static bool has_seen_setup = false;
static rimeaddr_t best_parent = {}, collecting_best_parent = {};
static uint32_t best_hop = UINT32_MAX, collecting_best_hop = UINT32_MAX;

PROCESS(tree_setup_process, "Aggregation Tree Setup");
PROCESS(send_data_process, "Data Sender");

static void parent_detect_finished(void * ptr)
{
	struct broadcast_conn * conn = (struct broadcast_conn *)ptr;

	printf("Timer on %d.%d expired\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

	// Set the best values
	best_parent = collecting_best_parent;
	best_hop = collecting_best_hop;

	printf("Found: Parent:%d.%d Hop:%u\n", best_parent.u8[0], best_parent.u8[1], best_hop);

	setup_tree_msg_t * nextmsg;

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	nextmsg = (setup_tree_msg_t *)packetbuf_dataptr();

	nextmsg->base.type = setup_message_type;
	rimeaddr_copy(&nextmsg->id, &rimeaddr_node_addr);
	rimeaddr_copy(&nextmsg->parent, &best_parent);
	nextmsg->hop_count = best_hop + 1;

	broadcast_send(conn);

	process_start(&send_data_process, NULL);
}

/** The function that will be executed when a message is received */
static void recv(struct broadcast_conn * ptr, rimeaddr_t const * originator)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case collect_message_type:
		{
			collect_msg_t * msg = (collect_msg_t *)bmsg;

			// If the destination is this node
			if (rimeaddr_cmp(&msg->destination, &rimeaddr_node_addr) != 0)
			{
				printf("Network Data: Addr:%d.%d Temp:%d Hudmid:%d%%\n",
					originator->u8[0], originator->u8[1],
					(int)msg->temperature, (int)msg->humidity
				);

				// TODO:
				// Apply some aggregation function

			
				// Forward message onwards
				rimeaddr_copy(&msg->destination, &best_parent);
		
				broadcast_send(ptr);
			}
		} break;

		case setup_message_type:
		{
			setup_tree_msg_t const * msg = (setup_tree_msg_t const *)bmsg;

			printf("Got setup message from %d.%d\n", originator->u8[0], originator->u8[1]);

			if (!has_seen_setup)
			{
				has_seen_setup = true;
				ctimer_set(&ct, 15 * CLOCK_SECOND, &parent_detect_finished, ptr);

				printf("Not seen setup message before, so setting timer...\n");
			}

			if (msg->hop_count < best_hop)
			{
				// Set the best parent, and the hop count of that node
				rimeaddr_copy(&collecting_best_parent, &msg->id);
				collecting_best_hop = msg->hop_count;

				printf("Updating to a better parent (%d.%d)\n", collecting_best_parent.u8[0], collecting_best_parent.u8[1]);
			}
		} break;

		default:
		{
			printf("Unknown message type %u\n", bmsg->type);
		} break;
	}
}

/** List of all functions to execute when a message is received */
static const struct broadcast_callbacks callbacks = { recv };

AUTOSTART_PROCESSES(&tree_setup_process);

PROCESS_THREAD(tree_setup_process, ev, data)
{
	static struct broadcast_conn bc;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	broadcast_open(&bc, 128, &callbacks);

	if (is_sink())
	{
		setup_tree_msg_t * msg;

		packetbuf_clear();
		packetbuf_set_datalen(sizeof(setup_tree_msg_t));
		msg = (setup_tree_msg_t *)packetbuf_dataptr();

		msg->base.type = setup_message_type;
		rimeaddr_copy(&msg->id, &rimeaddr_node_addr);
		rimeaddr_copy(&msg->parent, &rimeaddr_null);
		msg->hop_count = 0;
	
		broadcast_send(&bc);

		printf("IsSink, sending initial message...\n");
	}

exit:
//	broadcast_close(&bc);
	(void)0;
	PROCESS_END();
}


PROCESS_THREAD(send_data_process, ev, data)
{
	static struct broadcast_conn bc;

	static struct etimer et;
	static unsigned raw_humidity, raw_temperature;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// By this point the tree should be set up,
	// so now we should move to aggregating data
	// through the tree

	etimer_set(&et, 3 * CLOCK_SECOND);
 
	while (true)
	{
		collect_msg_t * msg;

		PROCESS_YIELD();

		SENSORS_ACTIVATE(sht11_sensor);

		raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

		SENSORS_DEACTIVATE(sht11_sensor);

		/* http://arduino.cc/forum/index.php?topic=37752.5;wap2 */

		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		msg = (collect_msg_t *)packetbuf_dataptr();

		msg->base.type = collect_message_type;
		
		rimeaddr_copy(&msg->destination, &best_parent);
		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);
		
		broadcast_send(&bc);

		etimer_reset(&et);
	}
 
exit:
	broadcast_close(&bc);
	PROCESS_END();
}

