#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"
#include "dev/battery-sensor.h"

#include "dev/leds.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/broadcast.h"
#include "contiki-net.h"

#include "../Common/sensor-converter.h"
#include "../Common/debug-helper.h"


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

	rimeaddr_t parent;
	uint32_t hop_count;

} setup_tree_msg_t;

static struct ctimer ct;
static struct ctimer aggregate_ct;

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

	printf("Timer on %s expired\n",
		addr2str(&rimeaddr_node_addr));

	// Set the best values
	best_parent = collecting_best_parent;
	best_hop = collecting_best_hop;

	printf("Found: Parent:%s Hop:%u\n",
		addr2str(&best_parent), best_hop);

	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	debug_packet_size(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * nextmsg = (setup_tree_msg_t *)packetbuf_dataptr();
	memset(nextmsg, 0, sizeof(setup_tree_msg_t));

	// We set the parent of this node to be the best
	// parent we heard
	nextmsg->base.type = setup_message_type;
	rimeaddr_copy(&nextmsg->parent, &best_parent);
	nextmsg->hop_count = best_hop + 1;

	broadcast_send(conn);

	// We are done with setting up the tree
	// so stop listening for setup messages
	//broadcast_close(conn);

	// Start the data generation process
	process_start(&send_data_process, NULL);
}

static bool is_collecting = false;
static double aggregate_temperature;
static double aggregate_humidity;

static bool is_leaf_node = true;

static void finish_aggregate_collect(void * ptr)
{
	SENSORS_ACTIVATE(sht11_sensor);
	unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
	unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	SENSORS_DEACTIVATE(sht11_sensor);

	double temp = sht11_temperature(raw_temperature);
	aggregate_temperature += temp;
	aggregate_humidity += sht11_relative_humidity_compensated(raw_humidity, temp);

	aggregate_temperature /= 2.0;
	aggregate_humidity /= 2.0;


	struct unicast_conn * conn = (struct unicast_conn *)ptr;

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(collect_msg_t));
	debug_packet_size(sizeof(collect_msg_t));
	collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(collect_msg_t));

	msg->base.type = collect_message_type;
	rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
	msg->temperature = aggregate_temperature;
	msg->humidity = aggregate_humidity;

	unicast_send(conn, &best_parent);

	printf("Send Agg: Addr:%s Dest:%s Temp:%d Hudmid:%d%%\n",
		addr2str(&rimeaddr_node_addr),
		addr2str(&best_parent),
		(int)aggregate_temperature, (int)aggregate_humidity
	);

	is_collecting = false;
	aggregate_temperature = 0;
	aggregate_humidity = 0;
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

			if (is_sink())
			{
				printf("Sink rcv: Addr:%s Src:%s Temp:%d Hudmid:%d%%\n",
					addr2str(originator),
					addr2str(&msg->base.source),
					(int)msg->temperature, (int)msg->humidity
				);
			}
			else
			{
				// Apply some aggregation function
				if (is_collecting)
				{
					printf("Cont Agg: Addr:%s Src:%s Temp:%d Hudmid:%d%%\n",
						addr2str(originator),
						addr2str(&msg->base.source),
						(int)msg->temperature, (int)msg->humidity
					);

					aggregate_temperature += msg->temperature;
					aggregate_humidity += msg->humidity;

					aggregate_temperature /= 2.0;
					aggregate_humidity /= 2.0;
				}
				else
				{
					printf("Star Agg: Addr:%s Src:%s Temp:%d Hudmid:%d%%\n",
						addr2str(originator),
						addr2str(&msg->base.source),
						(int)msg->temperature, (int)msg->humidity
					);

					aggregate_temperature = msg->temperature;
					aggregate_humidity = msg->humidity;

					is_collecting = true;

					ctimer_set(&aggregate_ct, 20 * CLOCK_SECOND, &finish_aggregate_collect, ptr);
				}
			}
	
		} break;

		default:
		{
			printf("Unknown message type %d (%s)\n", bmsg->type, message_type_to_string(bmsg->type));
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

			printf("Got setup message from %s\n", addr2str(originator));

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
				printf("Updating to a better parent (%s H:%u) was:(%s H:%u)\n",
					addr2str(originator), msg->hop_count,
					addr2str(&collecting_best_parent), collecting_best_hop
				);

				// Set the best parent, and the hop count of that node
				rimeaddr_copy(&collecting_best_parent, originator);
				collecting_best_hop = msg->hop_count;
			}

			
			// If the parent of the node that sent this message is this node,
			// then we are not a leaf
			if (is_leaf_node && rimeaddr_cmp(&msg->parent, &rimeaddr_node_addr) != 0)
			{
				printf("Node (%s) is our child, we are not a leaf.\n",
					addr2str(originator));

				is_leaf_node = false;
			}

		} break;

		default:
		{
			printf("Unknown message type %d (%s)\n", bmsg->type, message_type_to_string(bmsg->type));
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

			// Send the first message that will be used to set up the
			// aggregation tree
			packetbuf_clear();
			packetbuf_set_datalen(sizeof(setup_tree_msg_t));
			debug_packet_size(sizeof(setup_tree_msg_t));
			setup_tree_msg_t * msg = (setup_tree_msg_t *)packetbuf_dataptr();
			memset(msg, 0, sizeof(setup_tree_msg_t));

			msg->base.type = setup_message_type;
			rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
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
 
	// Only leaf nodes send these messages
	while (is_leaf_node)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Read the data from the temp and humidity sensors
		SENSORS_ACTIVATE(sht11_sensor);
		raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);


		// Create the data message that we are going to send
		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		debug_packet_size(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
		memset(msg, 0, sizeof(collect_msg_t));

		msg->base.type = collect_message_type;
		rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);

		printf("Generated new message to:(%s).\n",
			addr2str(&best_parent)
		);
		
		unicast_send(&uc, &best_parent);

		etimer_reset(&et);
	}
 
exit:
	//unicast_close(&uc);
	(void)0;
	PROCESS_END();
}

