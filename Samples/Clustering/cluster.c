#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"
#include "dev/battery-sensor.h"

#include "dev/leds.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/broadcast.h"
#include "contiki-net.h"

#include "sensor-converter.h"


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

typedef struct
{
	base_msg_t base;

	double temperature;
	double humidity;
} collect_msg_t;

typedef struct
{
	base_msg_t base;

	rimeaddr_t head;
	uint32_t hop_count;
	uint32_t round;
	bool from_CH;

} setup_tree_msg_t;


static struct ctimer ct;
static struct ctimer aggregate_ct;

static bool has_seen_setup = false;
static rimeaddr_t best_CH, collecting_best_CH, sink;
static uint32_t best_hop = UINT32_MAX, collecting_best_hop = UINT32_MAX;

static float p = 0.5f;				// Percentage of clusterheads
static unsigned round = 0;			// Round of CH selection
static int lastR = -50;				// Last round selected
static bool is_CH = false;


PROCESS(ch_election_process, "Cluster Setup");
PROCESS(send_data_process, "Data Sender");
PROCESS(sink_wait_process, "Sink Wait");


static bool elect_clusterhead()
{
	if ((round - lastR) > (1.0f / p))
	{
		// TODO: Find a good seed
		int randno = rand() % 100;
		int threshold = (int)(100 * p);

		printf("Eligible for CH, electing... %d < %d?\n", randno, threshold);

		return randno < threshold;
	}
	return false;
}


static void CH_detect_finished(void * ptr)
{
	struct broadcast_conn * conn = (struct broadcast_conn *)ptr;

	// As we are no longer listening for our parent node
	// indicate so through the LEDs
	leds_off(LEDS_RED);

	printf("Timer on %d.%d expired\n",
		rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

	// Set the best values
	best_CH = collecting_best_CH;
	best_hop = collecting_best_hop;

	printf("Found: Parent:%u.%u Hop:%u\n",
		best_CH.u8[0], best_CH.u8[1], best_hop);

	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * nextmsg = (setup_tree_msg_t *)packetbuf_dataptr();

	// We set the parent of this node to be the best
	// parent we heard
	nextmsg->base.type = setup_message_type;
	rimeaddr_copy(&nextmsg->head, &best_CH);
	nextmsg->hop_count = best_hop + 1;

	if (is_CH)
		nextmsg->from_CH = true;
	else
		nextmsg->from_CH = false;
	
	broadcast_send(conn);
	has_seen_setup = false;		// Prepare for next round

	// We are done with setting up the tree
	// so stop listening for setup messages
	//broadcast_close(conn);

	// Start the data generation process
	process_start(&send_data_process, NULL);
}

static bool is_collecting = false;
static double aggregate_temperature = 0;
static double aggregate_humidity = 0;
static uint32_t collected = 1;

//static bool is_leaf_node = true;

static void finish_aggregate_collect(void * ptr)
{
	SENSORS_ACTIVATE(sht11_sensor);
	unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
	unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	SENSORS_DEACTIVATE(sht11_sensor);

	double temp = sht11_temperature(raw_temperature);
	aggregate_temperature += temp;
	aggregate_humidity += sht11_relative_humidity_compensated(raw_humidity, temp);

	aggregate_temperature /= (double)collected;
	aggregate_humidity /= (double)collected;
	collected = 1;


	struct unicast_conn * conn = (struct unicast_conn *)ptr;

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(collect_msg_t));
	collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();

	msg->base.type = collect_message_type;
	rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
	msg->temperature = aggregate_temperature;
	msg->humidity = aggregate_humidity;

	unicast_send(conn, &best_CH);

	printf("Send Agg: Addr:%d.%d Dest:%d.%d Temp:%d Hudmid:%d%%\n",
		rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
		best_CH.u8[0], best_CH.u8[1],
		(int)aggregate_temperature, (int)aggregate_humidity
	);

	is_collecting = false;
	aggregate_temperature = 0;
	aggregate_humidity = 0;
}

static void new_round()
{
	printf("New round: %u!\n", round + 1);
	process_start(&ch_election_process, NULL);
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
				printf("Sink rcv: Addr:%d.%d Src:%d.%d Temp:%d Hudmid:%d%%\n",
					originator->u8[0], originator->u8[1],
					msg->base.source.u8[0], msg->base.source.u8[1],
					(int)msg->temperature, (int)msg->humidity
				);
			}
			else
			{
				// Apply some aggregation function
				if (is_collecting)
				{
					printf("Cont Agg: Addr:%d.%d Src:%d.%d Temp:%d Hudmid:%d%%\n",
						originator->u8[0], originator->u8[1],
						msg->base.source.u8[0], msg->base.source.u8[1],
						(int)msg->temperature, (int)msg->humidity
					);

					aggregate_temperature += msg->temperature;
					aggregate_humidity += msg->humidity;
					collected++;
				}
				else
				{
					printf("Star Agg: Addr:%d.%d Src:%d.%d Temp:%d Hudmid:%d%%\n",
						originator->u8[0], originator->u8[1],
						msg->base.source.u8[0], msg->base.source.u8[1],
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

			printf("Got setup message from %d.%d\n", originator->u8[0], originator->u8[1]);

			// If the sink received a setup message, then do nothing
			// it doesn't need a parent as it is the root.
			if (is_sink())
			{
				printf("We are the sink node, so should not listen for parents.\n");
				break;
			}

			// If this is the first setup message that we have seen this round
			// Then we need to start the collect timeout
			if (!has_seen_setup && msg->round > round)
			{
				has_seen_setup = true;
				round = msg->round;
				
				is_CH = elect_clusterhead();
				if (is_CH)
					leds_on(LEDS_BLUE);
				else
					leds_off(LEDS_BLUE);
					
				// Indicate that we are looking for best parent
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
				ctimer_set(&ct, 15 * CLOCK_SECOND, &CH_detect_finished, ptr);

				printf("Not seen round %d setup message before, so setting timer...\n",round);
			}

			// Record the node the message came from, if it is closer to the sink.
			// A CH should simply look for the best path to the sink, other nodes
			// should look for the closest CH.
			if (msg->hop_count < collecting_best_hop && (msg->from_CH || is_CH))
			{
				printf("Updating to a better clusterhead (%d.%d H:%d) was:(%d.%d H:%d)\n",
					originator->u8[0], originator->u8[1], msg->hop_count,
					collecting_best_CH.u8[0], collecting_best_CH.u8[1], collecting_best_hop
				);

				// Set the best parent, and the hop count of that node
				rimeaddr_copy(&collecting_best_CH, originator);
				collecting_best_hop = msg->hop_count;
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

AUTOSTART_PROCESSES(&ch_election_process);

PROCESS_THREAD(ch_election_process, ev, data)
{
	static struct etimer et;
	static struct broadcast_conn bc;
	static struct unicast_conn uc;
	static int i;

	PROCESS_BEGIN();

	printf("Setting up...\n");

	rimeaddr_copy(&best_CH, &rimeaddr_null);
	rimeaddr_copy(&collecting_best_CH, &rimeaddr_null);

	// Set sink
	memset(&sink, 0, sizeof(rimeaddr_t));
	sink.u8[sizeof(rimeaddr_t) - 2] = 1;


	broadcast_open(&bc, 128, &callbacks_setup);

	if (is_sink())
	{
		unicast_open(&uc, 114, &callbacks_aggregate);
		round++;
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
			setup_tree_msg_t *msg = (setup_tree_msg_t *)packetbuf_dataptr();

			msg->base.type = setup_message_type;
			rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
			rimeaddr_copy(&msg->head, &rimeaddr_null);
			msg->hop_count = 0;
			msg->round = round;
			msg->from_CH = false;
	
			broadcast_send(&bc);

			printf("IsSink, sending initial message (%d)...\n", i);

			etimer_reset(&et);
		}

		broadcast_close(&bc);
	}
	if(is_sink())
	{
		printf("End of setup process\n");
		process_start(&sink_wait_process,NULL);
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

	// By this point the clusterheads should be set up,
	// so now we should move to aggregating data
	// through the clusters

	etimer_set(&et, 20 * CLOCK_SECOND);
 
	// Only non-clusterhead nodes send these messages
	while (!is_CH)
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
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();

		msg->base.type = collect_message_type;
		rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);

		printf("Generated new message to:(%d.%d).\n",
			best_CH.u8[0], best_CH.u8[1]
		);
		
		unicast_send(&uc, &best_CH);

		etimer_reset(&et);
	}
 
exit:
	//unicast_close(&uc);
	(void)0;
	PROCESS_END();
}

PROCESS_THREAD(sink_wait_process, ev, data)
{
	PROCESS_BEGIN();
	printf("I'm waiting\n");
	ctimer_set(&ct, 300 * CLOCK_SECOND, &new_round,NULL);
	PROCESS_END();
}
