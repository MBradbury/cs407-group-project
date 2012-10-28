#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"
#include "dev/battery-sensor.h"

#include "dev/leds.h"
#include "dev/radio-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/broadcast.h"
#include "net/rime/timesynch.h"
#include "contiki-net.h"

static double sht11_relative_humidity(unsigned raw)
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

static double sht11_relative_humidity_compensated(unsigned raw, double temperature)
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
static double sht11_temperature(unsigned raw)
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

/** From: www.scribd.com/doc/73156710/Contiki-1 */
static double battery_voltage(unsigned raw)
{
	return (raw * 2.500 * 2.0) / 4096;
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

static const char * message_type_to_string(message_type_t type)
{
	switch (type)
	{
		case collect_message_type: return "Collect Message";
		case setup_message_type: return "Setup Message";
		default: return "Unknown Message";
	}
}


typedef struct
{
	message_type_t type;
	rimeaddr_t source;
} base_msg_t;

/** The structure of the message we are sending */
typedef struct
{
	base_msg_t base;

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

	// As we are no longer listening for our parent node
	// indicate so through the LEDs
	leds_off(LEDS_RED);

	printf("Timer on %d.%d expired\n",
		rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

	// Set the best values
	best_parent = collecting_best_parent;
	best_hop = collecting_best_hop;

	printf("Found: Parent:%d.%d Hop:%u\n",
		best_parent.u8[0], best_parent.u8[1], best_hop);

	/*SENSORS_ACTIVATE(radio_sensor);
	int radio_value_raw = radio_sensor.value(RADIO_SENSOR_LAST_VALUE);
	int radio_packet_raw = radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
	SENSORS_DEACTIVATE(radio_sensor);

	printf("Radio value %d -- %d\n", radio_value_raw, radio_packet_raw);*/

	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * nextmsg = (setup_tree_msg_t *)packetbuf_dataptr();

	// We set the parent of this node to be the best
	// parent we heard
	nextmsg->base.type = setup_message_type;
	rimeaddr_copy(&nextmsg->id, &rimeaddr_node_addr);
	rimeaddr_copy(&nextmsg->parent, &best_parent);
	nextmsg->hop_count = best_hop + 1;

	broadcast_send(conn);

	// We are done with setting up the tree
	// so stop listening for setup messages
	broadcast_close(conn);

	// Start the data generation process
	process_start(&send_data_process, NULL);
}

/** The function that will be executed when a message is received */
static void recv_aggregate(struct unicast_conn * ptr, rimeaddr_t const * originator)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case collect_message_type:
		{
			collect_msg_t const * msg = (collect_msg_t const *)bmsg;

			printf("Network Data: Addr:%d.%d Src:%d.%d Temp:%d Hudmid:%d%%\n",
				originator->u8[0], originator->u8[1],
				msg->base.source.u8[0], msg->base.source.u8[1],
				(int)msg->temperature, (int)msg->humidity
			);

			// TODO:
			// Apply some aggregation function

		
			// Forward message onwards, if not the sink
			if (!is_sink())
			{
				/*SENSORS_ACTIVATE(radio_sensor);
				int radio_value_raw = radio_sensor.value(RADIO_SENSOR_LAST_VALUE);
				int radio_packet_raw = radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
				SENSORS_DEACTIVATE(radio_sensor);

				printf("Radio value %d -- %d\n", radio_value_raw, radio_packet_raw);*/

				unicast_send(ptr, &best_parent);
			}
		} break;

		default:
		{
			printf("Unknown message type %u (%s)\n", bmsg->type, message_type_to_string(bmsg->type));
		} break;
	}
}

/** The function that will be executed when a message is received */
static void recv_setup(struct broadcast_conn * ptr, rimeaddr_t const * originator)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case setup_message_type:
		{
			setup_tree_msg_t const * msg = (setup_tree_msg_t const *)bmsg;

			printf("Got setup message from %d.%d\n", originator->u8[0], originator->u8[1]);

			// If the sink received a setup message, then do nothing
			// it doesn't need a parent as it is the root.
			if (is_sink())
			{
				printf("We are the sink node, so should not listen for parents.\n");
				break;
			}

			// If this is the first setup message that we have seen
			// Then we need to start the collect timeout
			if (!has_seen_setup)
			{
				has_seen_setup = true;

				// Indicate that we are setting up
				leds_on(LEDS_RED);


				// TODO: In the future the time to wait for
				// parents should be related to the battery remaining.
				// The more power remaining, the more time should be
				// spent listening
				/*SENSORS_ACTIVATE(battery_sensor);
				unsigned battery_raw = battery_sensor.value(0);
				SENSORS_DEACTIVATE(battery_sensor);

				printf("Battery value Raw:%u Real:%dV\n", battery_raw, (int)battery_voltage(battery_raw));*/

				// Start the timer that will call a function when we are
				// done detecting parents.
				ctimer_set(&ct, 15 * CLOCK_SECOND, &parent_detect_finished, ptr);

				printf("Not seen setup message before, so setting timer...\n");
			}

			// As we have received a message we need to record the node
			// it came from, if it is closer to the sink.
			if (msg->hop_count < collecting_best_hop)
			{
				printf("Updating to a better parent (%d.%d H:%d) was:(%d.%d H:%d)\n",
					msg->id.u8[0], msg->id.u8[1], msg->hop_count,
					collecting_best_parent.u8[0], collecting_best_parent.u8[1], collecting_best_hop
				);

				// Set the best parent, and the hop count of that node
				rimeaddr_copy(&collecting_best_parent, &(msg->id));
				collecting_best_hop = msg->hop_count;
			}
		} break;

		default:
		{
			printf("Unknown message type %u (%s)\n", bmsg->type, message_type_to_string(bmsg->type));
		} break;
	}
}

/** List of all functions to execute when a message is received */
static const struct broadcast_callbacks callbacks_setup = { recv_setup };
static const struct unicast_callbacks callbacks_aggregate = { recv_aggregate };

AUTOSTART_PROCESSES(&tree_setup_process);

PROCESS_THREAD(tree_setup_process, ev, data)
{
	static struct etimer et;
	static struct broadcast_conn bc;
	static struct unicast_conn uc;
	static int i;

	PROCESS_BEGIN();

	broadcast_open(&bc, 128, &callbacks_setup);

	if (is_sink())
	{
		unicast_open(&uc, 114, &callbacks_aggregate);

		leds_on(LEDS_YELLOW);

		etimer_set(&et, 4 * CLOCK_SECOND);

		// Retry initial send a few times to make sure
		// that the surrounding nodes will get a message
		for (i = 0; i != 2; ++i)
		{
			PROCESS_YIELD();

			/*SENSORS_ACTIVATE(radio_sensor);
			int radio_value_raw = radio_sensor.value(RADIO_SENSOR_LAST_VALUE);
			int radio_packet_raw = radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
			SENSORS_DEACTIVATE(radio_sensor);

			printf("Radio value %d -- %d\n", radio_value_raw, radio_packet_raw);*/

			setup_tree_msg_t * msg;

			// Send the first message that will be used to set up the
			// aggregation tree
			packetbuf_clear();
			packetbuf_set_datalen(sizeof(setup_tree_msg_t));
			msg = (setup_tree_msg_t *)packetbuf_dataptr();

			msg->base.type = setup_message_type;
			rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
			rimeaddr_copy(&msg->id, &rimeaddr_node_addr);
			rimeaddr_copy(&msg->parent, &rimeaddr_null);
			msg->hop_count = 0;
	
			broadcast_send(&bc);

			printf("IsSink, sending initial message (%d)...\n", i);

			etimer_reset(&et);
		}

		broadcast_close(&bc);
	}

	PROCESS_END();
}


PROCESS_THREAD(send_data_process, ev, data)
{
	static struct unicast_conn uc;

	static struct etimer et;
	static unsigned raw_humidity, raw_temperature;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	printf("Starting data generation process\n");

	leds_on(LEDS_GREEN);

	unicast_open(&uc, 114, &callbacks_aggregate);

	// By this point the tree should be set up,
	// so now we should move to aggregating data
	// through the tree

	etimer_set(&et, 20 * CLOCK_SECOND);
 
	while (true)
	{
		collect_msg_t * msg;

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Read the data from the temp and humidity sensors
		SENSORS_ACTIVATE(sht11_sensor);
		raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		/*SENSORS_ACTIVATE(radio_sensor);
		int radio_value_raw = radio_sensor.value(RADIO_SENSOR_LAST_VALUE);
		int radio_packet_raw = radio_sensor.value(RADIO_SENSOR_LAST_PACKET);
		SENSORS_DEACTIVATE(radio_sensor);

		printf("Radio value %d -- %d\n", radio_value_raw, radio_packet_raw);*/


		// Create the data message that we are going to send
		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		msg = (collect_msg_t *)packetbuf_dataptr();

		msg->base.type = collect_message_type;
		rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);
		
		unicast_send(&uc, &best_parent);

		etimer_reset(&et);
	}
 
exit:
	unicast_close(&uc);
	PROCESS_END();
}

