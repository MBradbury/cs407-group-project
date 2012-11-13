#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"
#include "dev/battery-sensor.h"

#include "dev/leds.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"
#include "net/rime/mesh.h"
#include "contiki-net.h"

#include "hcluster-var-depth.h"

#include "sensor-converter.h"
#include "debug-helper.h"

typedef struct
{
	rimeaddr_t source;

	rimeaddr_t head;
	uint32_t head_level;
	uint32_t hop_count;

} setup_msg_t;


static cluster_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (cluster_conn_t *)conn;
}

static cluster_conn_t * conncvt_mesh(struct mesh_conn * conn)
{
	return (cluster_conn_t *)(((char *)conn) - sizeof(struct stbroadcast_conn));
}

static cluster_conn_t * conncvt_runicast(struct runicast_conn * conn)
{
	return (cluster_conn_t *)
		(((char *)conn) - sizeof(struct stbroadcast_conn) - sizeof(struct mesh_conn));
}


static bool is_sink(cluster_conn_t const * conn)
{
	return rimeaddr_cmp(&conn->sink, &rimeaddr_node_addr) != 0;
}

static int delay()
{
	return ((int)rimeaddr_node_addr.u8[0]) % 5;
}


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


static void stbroadcast_cancel_void(void * ptr)
{
	stbroadcast_cancel(&conncvt_stbcast((struct stbroadcast_conn *)ptr)->bc);
	printf("Stubborn bcast cancelled\n");

	if (CLUSTER_DEPTH == 0)
	{
		leds_off(LEDS_BLUE);
	}
}

static void forward_setup(void * ptr)
{
	cluster_conn_t * conn = (cluster_conn_t *)ptr;
	static struct ctimer forward_stop;
	
	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_msg_t));
	setup_msg_t * nextmsg = (setup_msg_t *)packetbuf_dataptr();
	memset(nextmsg, 0, sizeof(setup_msg_t));

	// We set the head of this node to be the best
	// clusterhead we heard, or itself if is_CH
	rimeaddr_copy(&nextmsg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&nextmsg->head, conn->is_CH
		? &rimeaddr_node_addr
		: &conn->our_cluster_head);
	nextmsg->head_level = conn->our_level;
	nextmsg->hop_count = conn->is_CH ? 0 : conn->best_hop+1;
	
	printf("Forwarding setup message...\n");
	stbroadcast_send_stubborn(&conn->bc, STUBBORN_INTERVAL);
	
	ctimer_set(&forward_stop, STUBBORN_WAIT, &stbroadcast_cancel_void, conn);

	//Inform user that this node is set up
	(*conn->callbacks.setup_complete)(conn);
}

static void CH_detect_finished(void * ptr)
{
	cluster_conn_t * conn = (cluster_conn_t *)ptr;
	static struct ctimer message_offset;
	int offset = (delay()) * CLOCK_SECOND;

	// As we are no longer listening for our parent node
	// indicate so through the LEDs
	leds_off(LEDS_RED);

	printf("Timer on %s expired\n",	addr2str(&rimeaddr_node_addr));
	
	// Set the best values
	rimeaddr_copy(&conn->our_cluster_head, &conn->collecting_best_CH);	
	conn->best_hop = conn->collecting_best_hop;

	conn->is_CH = conn->best_hop == CLUSTER_DEPTH;

	conn->our_level = conn->is_CH ? conn->collecting_best_level+1 : conn->collecting_best_level;
	
	// Special case for first layer of CHs - always 1 hop from sink. This is definitely not a hack.
	if (conn->is_CH && conn->our_level == 1)
	{
		conn->best_hop = 0;
	}
	
	printf("Found: Head:%s Level:%u, Hops:%u\n",
		addr2str(&conn->our_cluster_head), conn->collecting_best_level, conn->best_hop);
	if (conn->is_CH)
	{
		printf("I'm a level %u CH, come to me my children!\n",conn->our_level);
		leds_on(LEDS_BLUE);
	}

	ctimer_set(&message_offset, offset, &forward_setup, conn);
}


static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	printf("Runicast sent to: %s, retries: %u\n",
        addr2str(to), retransmissions);
}

static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	printf("Runicast timed out when sending to: %s, retries: %u\n",
        addr2str(to), retransmissions);
}

static void recv_runicast(struct runicast_conn * ptr, rimeaddr_t const * originator, uint8_t seqno)
{
	cluster_conn_t * conn = conncvt_runicast(ptr);

	// Extract the source we included at the end of the packet
	rimeaddr_t const * source = (rimeaddr_t const *)
		(((char *)packetbuf_dataptr()) + packetbuf_datalen() - sizeof(rimeaddr_t));

	if (is_sink(conn))
	{
		// Change the packet length to the expected length
		packetbuf_set_datalen(packetbuf_datalen() - sizeof(rimeaddr_t));

		(*conn->callbacks.recv)(conn, source);
	}else
	{
		// A cluster head has received a data message from a node in its cluster.
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
			addr2str_r(&conn->our_cluster_head, ch_str, RIMEADDR_STRING_LENGTH)
		);
		if (conn->best_hop==0)
		{
			// Include the originator in the header
			rimeaddr_t * source = (rimeaddr_t *)(((char *)packetbuf_dataptr()) + packetbuf_datalen());
			packetbuf_set_datalen(packetbuf_datalen() + sizeof(rimeaddr_t));
			rimeaddr_copy(source, originator);
			runicast_send(&conn->rc, &conn->our_cluster_head, MAX_RUNICAST_RETX);
		}else
		{
			mesh_send(&conn->mc, &conn->our_cluster_head);
		}
	}
}

static const struct runicast_callbacks callbacks_forward = { &recv_runicast, &sent_runicast, &timedout_runicast };

/** The function that will be executed when a mesh message is received */
static void recv_mesh(struct mesh_conn * ptr, rimeaddr_t const * originator, uint8_t hops)
{
	cluster_conn_t * conn = conncvt_mesh(ptr);
	
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
		addr2str_r(&conn->our_cluster_head, ch_str, RIMEADDR_STRING_LENGTH)
	);
	if (conn->best_hop==0)
	{
		// Include the originator in the header
		rimeaddr_t * source = (rimeaddr_t *)(((char *)packetbuf_dataptr()) + packetbuf_datalen());
		packetbuf_set_datalen(packetbuf_datalen() + sizeof(rimeaddr_t));
		rimeaddr_copy(source, originator);

		runicast_send(&conn->rc, &conn->our_cluster_head, MAX_RUNICAST_RETX);
	}else
	{
		mesh_send(&conn->mc, &conn->our_cluster_head);
	}
}

static void mesh_sent(struct mesh_conn * c) {}
static void mesh_timedout(struct mesh_conn * c) {}

static const struct mesh_callbacks callbacks_data = { &recv_mesh, &mesh_sent, &mesh_timedout };


/** The function that will be executed when a message is received */
static void recv_setup(struct stbroadcast_conn * ptr)
{
	cluster_conn_t * conn = conncvt_stbcast(ptr);

	setup_msg_t const * msg = (setup_msg_t const *)packetbuf_dataptr();
	static struct ctimer detect_ct;

	printf("Got setup message from %s, level %u\n",
		addr2str(&msg->source), msg->head_level);

	// If the sink received a setup message, then do nothing
	// it doesn't need a parent as it is the root.
	if (is_sink(conn))
	{
		return;
	}

	// If this is the first setup message that we have seen
	// Then we need to start the collect timeout
	if (!conn->has_seen_setup)
	{
		conn->has_seen_setup = true;
		
		conn->collecting_best_level = msg->head_level;
		conn->collecting_best_CH = msg->head;
		conn->collecting_best_hop = msg->hop_count;

		// Indicate that we are looking for best parent
		leds_on(LEDS_RED);

		// Start the timer that will call a function when we are
		// done detecting clusterheads.
		ctimer_set(&detect_ct, 20 * CLOCK_SECOND, &CH_detect_finished, conn);

		printf("Not seen setup message before, so setting timer...\n");
	}
	
	// Record the node the message came from, if it is closer to the sink.
	// Non-CH nodes should look for the closest CH.
	if (msg->hop_count < conn->collecting_best_hop && msg->head_level <= conn->collecting_best_level)
	{
		char head_str[RIMEADDR_STRING_LENGTH];
		char ch_str[RIMEADDR_STRING_LENGTH];

		printf("Updating to a better clusterhead (%s L:%d) was:(%s L:%d)\n",
			addr2str_r(&msg->head, head_str, RIMEADDR_STRING_LENGTH), msg->hop_count,
			addr2str_r(&conn->collecting_best_CH, ch_str, RIMEADDR_STRING_LENGTH), conn->collecting_best_hop
		);

		// Set the best parent, and the level of that node
		rimeaddr_copy(&conn->collecting_best_CH, &msg->head);
		conn->collecting_best_level = msg->head_level;
		conn->collecting_best_hop = msg->hop_count;
	}

}

static void sent_stbroadcast(struct stbroadcast_conn * c)
{
	printf("stBroadcast sent\n");
}

static const struct stbroadcast_callbacks callbacks_setup = { &recv_setup, &sent_stbroadcast };


static void CH_setup_wait_finished(void * ptr)
{
	cluster_conn_t * conn = (cluster_conn_t *)ptr;

	printf("Starting cluster setup...\n");

	leds_on(LEDS_YELLOW);

	// Send the first message that will be used to set up the
	// aggregation tree
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_msg_t));
	debug_packet_size(sizeof(setup_msg_t));
	setup_msg_t * msg = (setup_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(setup_msg_t));

	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->head, &rimeaddr_node_addr);
	msg->head_level = conn->our_level;
	msg->hop_count = CLUSTER_DEPTH;

	stbroadcast_send_stubborn(&conn->bc, STUBBORN_INTERVAL);

	printf("IsSink, sending initial message...\n");

	// Wait for a bit to allow a few messages to be sent
	static struct ctimer ct;
	ctimer_set(&ct, STUBBORN_WAIT, &stbroadcast_cancel_void, conn);
}


bool cluster_open(cluster_conn_t * conn, rimeaddr_t const * sink,
                  uint16_t ch1, uint16_t ch2, uint16_t ch3,
                  cluster_callbacks_t const * callbacks)
{
	if (conn != NULL && sink != NULL &&
		callbacks != NULL && callbacks->recv != NULL && callbacks->setup_complete != NULL)
	{
		stbroadcast_open(&conn->bc, ch1, &callbacks_setup);
		mesh_open(&conn->mc, ch2, &callbacks_data);
		runicast_open(&conn->rc, ch3, &callbacks_forward);

		rimeaddr_copy(&conn->our_cluster_head, &rimeaddr_null);
		rimeaddr_copy(&conn->collecting_best_CH, &rimeaddr_null);

		rimeaddr_copy(&conn->sink, sink);

		conn->has_seen_setup = false;

		conn->best_hop = UINT_MAX;
		conn->collecting_best_hop = UINT_MAX;
		conn->collecting_best_level = UINT_MAX;

		conn->is_CH = false;

		memcpy(&conn->callbacks, callbacks, sizeof(cluster_callbacks_t));

		if (is_sink(conn))
		{
			// Wait a bit to allow processes to start up
			static struct ctimer ct;
			ctimer_set(&ct, 10 * CLOCK_SECOND, &CH_setup_wait_finished, conn);
		}

		return true;
	}
	else
	{
		return false;
	}
}

void cluster_close(cluster_conn_t * conn)
{
	if (conn != NULL)
	{
		stbroadcast_close(&conn->bc);
		mesh_close(&conn->mc);
		runicast_close(&conn->rc);
	}
}


void cluster_send(cluster_conn_t * conn)
{
	// Prevent sink from sending any messages
	if (conn != NULL)
	{
		printf("Generated new message to:(%s).\n",
			addr2str(&conn->our_cluster_head));
		
		if (is_sink(conn))
		{
			// We are the sink, so just call the receive function
			(*conn->callbacks.recv)(conn, &rimeaddr_node_addr);
		}
		else if (conn->best_hop==0)
		{
			// The node is within range of its clusterhead/sink, so use runicast
		
			// Include the originator in the header
			rimeaddr_t * source = (rimeaddr_t *)(((char *)packetbuf_dataptr()) + packetbuf_datalen());
			packetbuf_set_datalen(packetbuf_datalen() + sizeof(rimeaddr_t));
			rimeaddr_copy(source, &rimeaddr_node_addr);

			runicast_send(&conn->rc, &conn->our_cluster_head, MAX_RUNICAST_RETX);
		}
		else
		{
			// Otherwise just use mesh to send the message to the CH
			mesh_send(&conn->mc, &conn->our_cluster_head);
		}
	}
}


/********************************************
 ********* APPLICATION BEGINS HERE **********
 *******************************************/


PROCESS(startup_process, "Startup");
PROCESS(send_data_process, "Data Sender");

AUTOSTART_PROCESSES(&startup_process);

typedef struct
{
	double temperature;
	double humidity;
} collect_msg_t;


static void cluster_recv(cluster_conn_t * conn, rimeaddr_t const * source)
{
	collect_msg_t const * msg = (collect_msg_t const *)packetbuf_dataptr();

	printf("Sink rcv: Src:%s Temp:%d Hudmid:%d%%\n",
			addr2str(source),
			(int)msg->temperature, (int)msg->humidity
	);
}

static void cluster_setup_finished(cluster_conn_t * conn)
{
	if (!is_sink(conn))
	{
		process_start(&send_data_process, (char *)conn);
	}
}

static cluster_conn_t conn;
static cluster_callbacks_t callbacks = { &cluster_recv, &cluster_setup_finished };

PROCESS_THREAD(startup_process, ev, data)
{
	static rimeaddr_t sink;

	PROCESS_BEGIN();

	sink.u8[0] = 1;
	sink.u8[1] = 0;

	cluster_open(&conn, &sink, 118, 132, 147, &callbacks);

	PROCESS_END();
}

PROCESS_THREAD(send_data_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	if (!is_sink(&conn))
	{
		printf("Starting data generation process\n");

		leds_on(LEDS_GREEN);

		// Send every minute.
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

			msg->temperature = temperature;
			msg->humidity = humidity;
			
			cluster_send(&conn);

			etimer_reset(&et);
		}
	}
	
	PROCESS_END();
}


