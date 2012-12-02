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

#include "cluster.h"

#include "sensor-converter.h"
#include "debug-helper.h"

typedef struct
{
	rimeaddr_t source;
	rimeaddr_t head;
	unsigned int hop_count;

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


// The maximum number of times the reliable unicast
// will attempt to resend a message.
static const int MAX_RUNICAST_RETX = 5;

// The times stubborn broadcasting will use
// to intersperse message resends
static const clock_time_t STUBBORN_INTERVAL = 4 * CLOCK_SECOND;
static const clock_time_t STUBBORN_WAIT = 30 * CLOCK_SECOND;


static void stbroadcast_cancel_void(void * ptr)
{
	stbroadcast_cancel(&conncvt_stbcast((struct stbroadcast_conn *)ptr)->bc);

	printf("Stubborn bcast canceled\n");
}


static void CH_detect_finished(void * ptr)
{
	cluster_conn_t * conn = (cluster_conn_t *)ptr;

	// As we are no longer listening for our parent node
	// indicate so through the LEDs
	leds_off(LEDS_RED);

	printf("Timer on %s expired\n", addr2str(&rimeaddr_node_addr));

	// Set the best values
	rimeaddr_copy(&conn->our_cluster_head, &conn->collecting_best_CH);
	conn->best_hop = conn->collecting_best_hop;

	printf("Found: Head:%s Hop:%u\n",
		addr2str(&conn->our_cluster_head), conn->best_hop);

	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_msg_t));
	setup_msg_t * msg = (setup_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(setup_msg_t));

	// We set the head of this node to be the best
	// clusterhead we heard, or itself if is_CH
	//nextmsg->base.type = setup_message_type;
	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->head, conn->is_CH
		? &rimeaddr_node_addr
		: &conn->our_cluster_head);
	msg->hop_count = conn->best_hop + 1;

	printf("Forwarding setup message...\n");
	stbroadcast_send_stubborn(&conn->bc, STUBBORN_INTERVAL);
	
	static struct ctimer forward_stop;
	ctimer_set(&forward_stop, STUBBORN_WAIT, &stbroadcast_cancel_void, conn);

	// The setup of this node is complete, so inform the user
	(*conn->callbacks.setup_complete)(conn);
}


/** The function that will be executed when a message is received */
static void runicast_recv(struct runicast_conn * ptr,
							rimeaddr_t const * originator, uint8_t seqno)
{
	cluster_conn_t * conn = conncvt_runicast(ptr);

	// Extract the source we included at the end of the packet
	rimeaddr_t const * source = (rimeaddr_t const *)
		(((char *)packetbuf_dataptr()) + packetbuf_datalen() - sizeof(rimeaddr_t));

	// Change the packet length to the expected length
	packetbuf_set_datalen(packetbuf_datalen() - sizeof(rimeaddr_t));

	(*conn->callbacks.recv)(conn, source);
}

static void runicast_sent(struct runicast_conn *c,
							rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("runicast sent to:%s rtx:%u\n", addr2str(to), retransmissions);
}
static void runicast_timedout(struct runicast_conn *c,
								rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("runicast timedout to:%s rtx:%u\n", addr2str(to), retransmissions);
}

static const struct runicast_callbacks callbacks_forward =
	{ &runicast_recv, &runicast_sent, &runicast_timedout };

/** The function that will be executed when a message is received */
static void mesh_recv(struct mesh_conn * ptr, rimeaddr_t const * originator, uint8_t hops)
{
	cluster_conn_t * conn = conncvt_mesh(ptr);

	// A cluster head has received a data message from
	// a member of its cluster.
	// We now need to forward it onto the sink.

	char originator_str[RIMEADDR_STRING_LENGTH];
	char current_str[RIMEADDR_STRING_LENGTH];
	char ch_str[RIMEADDR_STRING_LENGTH];

	printf("Forwarding: from:%s via:%s to:%s hops:%u\n",
		addr2str_r(originator, originator_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&rimeaddr_node_addr, current_str, RIMEADDR_STRING_LENGTH),
		addr2str_r(&conn->our_cluster_head, ch_str, RIMEADDR_STRING_LENGTH),
		hops
	);

	// Include the originator in the header
	rimeaddr_t * source = (rimeaddr_t *)(((char *)packetbuf_dataptr()) + packetbuf_datalen());
	packetbuf_set_datalen(packetbuf_datalen() + sizeof(rimeaddr_t));
	rimeaddr_copy(source, originator);

	runicast_send(&conn->rc, &conn->our_cluster_head, MAX_RUNICAST_RETX);
}

static void mesh_sent(struct mesh_conn * c) { }
static void mesh_timedout(struct mesh_conn * c) { }

static const struct mesh_callbacks callbacks_data = { &mesh_recv, &mesh_sent, &mesh_timedout };



/** The function that will be executed when a message is received */
static void recv_setup(struct stbroadcast_conn * ptr)
{
	cluster_conn_t * conn = conncvt_stbcast(ptr);

	setup_msg_t const * msg = (setup_msg_t const *)packetbuf_dataptr();

	printf("Got setup message from %s, %u hops away\n",
		addr2str(&msg->source), msg->hop_count);

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
		
		// We are the CH if the set up message
		// was sent by the sink
		conn->is_CH = msg->hop_count == 0;

		if (conn->is_CH)
		{
			// Turn blue on to indicate cluster head
			leds_on(LEDS_BLUE);

			printf("I'm a CH, come to me my children!\n");

			// We are the CH, so just call that the CH has
			// been detected
			conn->collecting_best_hop = 0;

			memset(&conn->collecting_best_CH, 0, sizeof(rimeaddr_t));
			conn->collecting_best_CH.u8[sizeof(rimeaddr_t) - 2] = 1;

			CH_detect_finished(conn);
		}
		else
		{
			leds_off(LEDS_BLUE);

			// Indicate that we are looking for best parent
			leds_on(LEDS_RED);

			// Start the timer that will call a function when we are
			// done detecting parents.
			// We only need to detect parents if we are not a cluster head
			static struct ctimer detect_ct;
			ctimer_set(&detect_ct, 20 * CLOCK_SECOND, &CH_detect_finished, conn);
		}

		printf("Not seen setup message before, so setting timer...\n");
	}
	
	// Record the node the message came from, if it is closer to the sink.
	// Non-CH nodes should look for the closest CH.
	if (msg->hop_count < conn->collecting_best_hop && !conn->is_CH)
	{
		char head_str[RIMEADDR_STRING_LENGTH];
		char ch_str[RIMEADDR_STRING_LENGTH];

		printf("Updating to a better clusterhead (%s H:%u) was:(%s H:%u)\n",
			addr2str_r(&msg->head, head_str, RIMEADDR_STRING_LENGTH), msg->hop_count,
			addr2str_r(&conn->collecting_best_CH, ch_str, RIMEADDR_STRING_LENGTH), conn->collecting_best_hop
		);

		// Set the best parent, and the hop count of that node
		rimeaddr_copy(&conn->collecting_best_CH, &msg->head);
		conn->collecting_best_hop = msg->hop_count;
	}
}

static void sent_stbroadcast(struct stbroadcast_conn * c)
{
	printf("sbcast sent\n");
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

	//msg->base.type = setup_message_type;
	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->head, &rimeaddr_null);
	msg->hop_count = 0;

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
		else if (conn->is_CH)
		{
			// The cluster head needs to use runicast to send
			// messages to the sink, so do so.

			// Include the originator in the header
			rimeaddr_t * source = (rimeaddr_t *)(((char *)packetbuf_dataptr()) + packetbuf_datalen());
			packetbuf_set_datalen(packetbuf_datalen() + sizeof(rimeaddr_t));
			rimeaddr_copy(source, &rimeaddr_node_addr);

			runicast_send(&conn->rc, &conn->sink, MAX_RUNICAST_RETX);
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

		// Send every second.
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

