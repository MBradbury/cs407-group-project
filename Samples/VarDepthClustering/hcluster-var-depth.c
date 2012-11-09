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
#include "net/rime/stbroadcast.h"
#include "contiki-net.h"
#include "net/rime/runicast.h"

#include "sensor-converter.h"
#include "debug-helper.h"

static bool is_sink()
{
	return rimeaddr_node_addr.u8[0] == 1 &&
		rimeaddr_node_addr.u8[1] == 0;
}

static int delay()
{
	return ((int)rimeaddr_node_addr.u8[0]) % 5;
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

	uint32_t head_level;
	rimeaddr_t head;
	uint32_t hop_count;

} setup_msg_t;


static struct mesh_conn mc;
static struct runicast_conn rc;
static struct stbroadcast_conn bc;

static rimeaddr_t our_cluster_head;
static uint32_t our_level;


static bool has_seen_setup = false;
static rimeaddr_t collecting_best_CH;
static uint32_t collecting_best_level = UINT32_MAX, collecting_best_hop = UINT32_MAX, best_hop = UINT32_MAX;

static bool is_CH = false;

// The maximum number of times the reliable unicast
// will attempt to resend a message.
static const int MAX_RUNICAST_RETX = 4;

// The number of non-CH nodes between CHs at
// consecutive levels
static const int CLUSTER_DEPTH = 2;

// The times stubborn broadcasting will use
// to intersperse message resends
static const clock_time_t STUBBORN_INTERVAL = 5 * CLOCK_SECOND;
static const clock_time_t STUBBORN_WAIT = 30 * CLOCK_SECOND;

PROCESS(startup, "Startup");
PROCESS(ch_election_process, "Cluster Setup");
PROCESS(send_data_process, "Data Sender");


static void stbroadcast_cancel_void(void * ptr)
{
	stbroadcast_cancel((struct stbroadcast_conn *)ptr);
	if (CLUSTER_DEPTH == 0)
	{
		leds_off(LEDS_BLUE);
	}
}

static void forward_setup()
{
	static struct ctimer forward_stop;
	
	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_msg_t));
	setup_msg_t * nextmsg = (setup_msg_t *)packetbuf_dataptr();
	memset(nextmsg, 0, sizeof(setup_msg_t));

	// We set the head of this node to be the best
	// clusterhead we heard, or itself if is_CH
	nextmsg->base.type = setup_message_type;
	rimeaddr_copy(&nextmsg->base.source, &rimeaddr_node_addr);
	rimeaddr_copy(&nextmsg->head, is_CH ? &rimeaddr_node_addr : &our_cluster_head);
	nextmsg->head_level = our_level;
	nextmsg->hop_count = is_CH ? 0 : best_hop+1;
	
	printf("Forwarding setup message...\n");
	stbroadcast_send_stubborn(&bc, STUBBORN_INTERVAL);
	
	ctimer_set(&forward_stop, STUBBORN_WAIT, &stbroadcast_cancel_void, &bc);

	// Start the data generation process for non-CHs
	if (!is_sink())
	{
		process_start(&send_data_process, NULL);
	}
}

static void CH_detect_finished(void * ptr)
{
	static struct ctimer message_offset;
	int offset = (delay()) * CLOCK_SECOND;

	// As we are no longer listening for our parent node
	// indicate so through the LEDs
	leds_off(LEDS_RED);

	// Set the best values
	printf("Timer on %s expired\n",
		addr2str(&rimeaddr_node_addr));
	
	rimeaddr_copy(&our_cluster_head, &collecting_best_CH);
	
	best_hop = collecting_best_hop;

	is_CH = best_hop == CLUSTER_DEPTH;

	our_level = is_CH ? collecting_best_level+1 : collecting_best_level;
	
	// Special case for first layer of CHs (always 1 hop from sink)
	if (is_CH && our_level == 1)
	{
		best_hop = 0;
	}
	
	printf("Found: Head:%s Level:%u, Hops:%u\n",
		addr2str(&our_cluster_head), collecting_best_level,best_hop);
	if (is_CH)
	{
		printf("I'm a level %u CH, come to me my children!\n",our_level);
		leds_on(LEDS_BLUE);
	}

	ctimer_set(&message_offset, offset, &forward_setup, NULL);
}


/** The function that will be executed when a runicast is sent */
static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	/*printf("Runicast message sent to %s, retransmissions %u\n",
         addr2str(to), retransmissions);*/
}

/** The function that will be executed when a runicast times out */
static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	/*printf("Runicast message timed out when sending to %s, retransmissions %u\n",
         addr2str(to), retransmissions);*/
}

/** The function that will be executed when a runicast message is received */
static void recv_runicast(struct runicast_conn * ptr, rimeaddr_t const * originator, uint8_t seqno)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();
	switch (bmsg->type)
	{
		case collect_message_type:
		{
			if (is_sink())
			{
				collect_msg_t const * msg = (collect_msg_t const *)bmsg;

				char originator_str[RIMEADDR_STRING_LENGTH];
				char source_str[RIMEADDR_STRING_LENGTH];

				printf("Sink rcv: Addr:%s Src:%s Temp:%d Hudmid:%d%%\n",
					addr2str_r(originator, originator_str, RIMEADDR_STRING_LENGTH),
					addr2str_r(&msg->base.source, source_str, RIMEADDR_STRING_LENGTH),
					(int)msg->temperature, (int)msg->humidity
				);
			}else
			{
				// A cluster head has received a data message from
				// a member of its cluster.
				// We now need to forward it onto the sink.

				char originator_str[RIMEADDR_STRING_LENGTH];
				char current_str[RIMEADDR_STRING_LENGTH];
				char ch_str[RIMEADDR_STRING_LENGTH];

				if (CLUSTER_DEPTH == 0)
				{
					leds_on(LEDS_BLUE);
				}
			
				printf("Forwarding: from:%s via:%s to:%s\n",
					addr2str_r(originator, originator_str, RIMEADDR_STRING_LENGTH),
					addr2str_r(&rimeaddr_node_addr, current_str, RIMEADDR_STRING_LENGTH),
					addr2str_r(&our_cluster_head, ch_str, RIMEADDR_STRING_LENGTH)
				);
				if (best_hop==0)
				{
					runicast_send(&rc, &our_cluster_head, MAX_RUNICAST_RETX);
				}else
				{
					mesh_send(&mc, &our_cluster_head);
				}
			}
		} break;

		default:
		{
			printf("Unknown message type\n");
		} break;
	}
}

/** The function that will be executed when a mesh message is received */
static void recv_mesh(struct mesh_conn * ptr, rimeaddr_t const * originator, uint8_t hops)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();
	switch (bmsg->type)
	{
		case collect_message_type:
		{
			// A cluster head has received a data message from
			// a member of its cluster.
			// We now need to forward it onto the sink.
			
			if (CLUSTER_DEPTH == 0)
			{
				leds_on(LEDS_BLUE);
			}
			
			char originator_str[RIMEADDR_STRING_LENGTH];
			char current_str[RIMEADDR_STRING_LENGTH];
			char ch_str[RIMEADDR_STRING_LENGTH];

			printf("Forwarding: from:%s via:%s to:%s\n",
				addr2str_r(originator, originator_str, RIMEADDR_STRING_LENGTH),
				addr2str_r(&rimeaddr_node_addr, current_str, RIMEADDR_STRING_LENGTH),
				addr2str_r(&our_cluster_head, ch_str, RIMEADDR_STRING_LENGTH)
			);
			if (best_hop==0)
			{
				runicast_send(&rc, &our_cluster_head, MAX_RUNICAST_RETX);
			}else
			{
				mesh_send(&mc, &our_cluster_head);
			}
	
		} break;

		default:
		{
			printf("Unknown message type\n");
		} break;
	}
}

static void mesh_sent(struct mesh_conn * c) {}
static void mesh_timedout(struct mesh_conn * c) {}

static const struct runicast_callbacks callbacks_forward = { &recv_runicast, &sent_runicast, &timedout_runicast };


/** The function that will be executed when a stbroadcast is sent */
static void sent_stbroadcast(struct stbroadcast_conn * c)
{
	//printf("stBroadcast message sent\n");
}


/** The function that will be executed when a message is received */
static void recv_setup(struct stbroadcast_conn * ptr)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case setup_message_type:
		{
			setup_msg_t const * msg = (setup_msg_t const *)bmsg;
			static struct ctimer detect_ct;

			printf("Got setup message from %s, level %u\n",
				addr2str(&bmsg->source), msg->head_level);

			// If the sink received a setup message, then do nothing
			// it doesn't need a parent as it is the root.
			if (is_sink())
			{
				break;
			}

			// If this is the first setup message that we have seen
			// Then we need to start the collect timeout
			if (!has_seen_setup)
			{
				has_seen_setup = true;
				
				collecting_best_level = msg->head_level;
				collecting_best_CH = msg->head;
				collecting_best_hop = msg->hop_count;

				// Cluster heads need runicast to forward
				// messages to the sink
				runicast_open(&rc, 147, &callbacks_forward);


				// Indicate that we are looking for best parent
				leds_on(LEDS_RED);

				// Start the timer that will call a function when we are
				// done detecting parents.
				// We only need to detect parents if we are not a cluster head
				ctimer_set(&detect_ct, 20 * CLOCK_SECOND, &CH_detect_finished, NULL);

				printf("Not seen setup message before, so setting timer...\n");
			}
			
			// Record the node the message came from, if it is closer to the sink.
			// Non-CH nodes should look for the closest CH.
			if (msg->hop_count < collecting_best_hop && msg->head_level <= collecting_best_level)
			{
				char head_str[RIMEADDR_STRING_LENGTH];
				char ch_str[RIMEADDR_STRING_LENGTH];

				printf("Updating to a better clusterhead (%s L:%d) was:(%s L:%d)\n",
					addr2str_r(&msg->head, head_str, RIMEADDR_STRING_LENGTH), msg->hop_count,
					addr2str_r(&collecting_best_CH, ch_str, RIMEADDR_STRING_LENGTH), collecting_best_hop
				);

				// Set the best parent, and the level of that node
				rimeaddr_copy(&collecting_best_CH, &msg->head);
				collecting_best_level = msg->head_level;
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
static const struct stbroadcast_callbacks callbacks_setup = { &recv_setup, &sent_stbroadcast };
static const struct mesh_callbacks callbacks_data = { &recv_mesh, &mesh_sent, &mesh_timedout };

AUTOSTART_PROCESSES(&startup);

PROCESS_THREAD(startup, ev, data)
{
	PROCESS_BEGIN();

	printf("Setting up...\n");

	rimeaddr_copy(&our_cluster_head, &rimeaddr_null);
	rimeaddr_copy(&collecting_best_CH, &rimeaddr_null);

	stbroadcast_open(&bc, 128, &callbacks_setup);
	if (CLUSTER_DEPTH > 0)
	{
		mesh_open(&mc, 114, &callbacks_data);
	}
	
	// May not need this, but have to open it here anyway
	// because C is a massive jeb-end
	runicast_open(&rc, 147, &callbacks_forward);
		
	if (is_sink())
	{
		our_level = 0;
		process_start(&ch_election_process, NULL);
	}

	PROCESS_END();
}

PROCESS_THREAD(ch_election_process, ev, data)
{
	static struct etimer et;
	
	PROCESS_BEGIN();
	
	// Wait a bit to allow nodes to start up
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	leds_on(LEDS_YELLOW);

	// Send the first message that will be used to set up the
	// cluster tree
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_msg_t));
	debug_packet_size(sizeof(setup_msg_t));
	setup_msg_t * msg = (setup_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(setup_msg_t));

	msg->base.type = setup_message_type;
	rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->head, &rimeaddr_node_addr);
	msg->head_level = our_level;
	msg->hop_count = CLUSTER_DEPTH;

	stbroadcast_send_stubborn(&bc, STUBBORN_INTERVAL);

	printf("IsSink, sending initial message...\n");

	// Wait for a bit to allow a few messages to be sent
	etimer_set(&et, STUBBORN_WAIT);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	// Cancel the stubborn send
	stbroadcast_cancel(&bc);

	printf("End of setup process\n");

	PROCESS_END();
}


PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	printf("Starting data generation process\n");

	leds_on(LEDS_GREEN);

	// By this point the clusterheads should be set up,
	// so now we should move to forwarding data
	// through the clusters

	etimer_set(&et, 60 * CLOCK_SECOND);
 
	while (true)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Read the data from the temp and humidity sensors
		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		double temperature = sht11_temperature(raw_temperature);
		double humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);


		// Create the data message that we are going to send
		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
		memset(msg, 0, sizeof(collect_msg_t));

		msg->base.type = collect_message_type;
		rimeaddr_copy(&msg->base.source, &rimeaddr_node_addr);
		msg->temperature = temperature;
		msg->humidity = humidity;

		printf("Generated new message to:(%s).\n",
			addr2str(&our_cluster_head));
		
		if (best_hop==0)
		{
			runicast_send(&rc, &our_cluster_head, MAX_RUNICAST_RETX);
		}else
		{
			mesh_send(&mc, &our_cluster_head);
		}

		etimer_reset(&et);
	}
 
	PROCESS_END();
}


