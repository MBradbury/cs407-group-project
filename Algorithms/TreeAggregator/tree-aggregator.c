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

#include "tree-aggregator.h"



static tree_agg_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (tree_agg_conn_t *)conn;
}

static tree_agg_conn_t * conncvt_unicast(struct unicast_conn * conn)
{
	return (tree_agg_conn_t *)
		(((char *)conn) - sizeof(struct stbroadcast_conn));
}



static bool is_sink(tree_agg_conn_t * conn)
{
	return rimeaddr_cmp(&conn->sink, &rimeaddr_node_addr) != 0;
}


// The maximum number of times the reliable unicast
// will attempt to resend a message.
static const int MAX_RUNICAST_RETX = 5;

// The times stubborn broadcasting will use
// to intersperse message resends
static const clock_time_t STUBBORN_INTERVAL = 5 * CLOCK_SECOND;
static const clock_time_t STUBBORN_WAIT = 30 * CLOCK_SECOND;

// Time to gather aggregations over
static const clock_time_t AGGREGATION_WAIT = 20 * CLOCK_SECOND;

// Time to wait to detect parents
static const clock_time_t PARENT_DETECT_WAIT = 15 * CLOCK_SECOND;


static void stbroadcast_cancel_void(void * ptr)
{
	stbroadcast_cancel(&conncvt_stbcast((struct stbroadcast_conn *)ptr)->bc);

	printf("Stubborn bcast canceled\n");
}

typedef struct
{
	rimeaddr_t source;
	rimeaddr_t parent;
	unsigned int hop_count;

} setup_tree_msg_t;


static void parent_detect_finished(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	// As we are no longer listening for our parent node
	// indicate so through the LEDs
	leds_off(LEDS_RED);

	printf("Timer on %s expired\n",
		addr2str(&rimeaddr_node_addr));

	// Set the best values
	conn->best_parent = conn->collecting_best_parent;
	conn->best_hop = conn->collecting_best_hop;

	printf("Found: Parent:%s Hop:%u\n",
		addr2str(&conn->best_parent), conn->best_hop);

	// Send a message that is to be received by the children
	// of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	debug_packet_size(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * msg = (setup_tree_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(setup_tree_msg_t));

	// We set the parent of this node to be the best
	// parent we heard
	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->parent, &conn->best_parent);
	msg->hop_count = conn->best_hop + 1;

	stbroadcast_send_stubborn(&conn->bc, STUBBORN_INTERVAL);

	// Wait for a bit to allow a few messages to be sent
	static struct ctimer ct;
	ctimer_set(&ct, STUBBORN_WAIT, &stbroadcast_cancel_void, conn);

	// Start the data generation process
	(*conn->callbacks.setup_complete)(conn);
}


static void finish_aggregate_collect(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	(*conn->callbacks.aggregate_own)(conn->data);

	// Copy aggregation data into the packet
	(*conn->callbacks.write_data_to_packet)(conn);

	unicast_send(&conn->uc, &conn->best_parent);

	printf("Send Agg\n");

	// We are no longer collecting aggregation data
	conn->is_collecting = false;

	// Reset the data we have stored to nothing
	memset(conn->data, 0, conn->data_length);
}

/** The function that will be executed when a message is received */
static void recv_aggregate(struct unicast_conn * ptr, rimeaddr_t const * originator)
{
	tree_agg_conn_t * conn = conncvt_unicast(ptr);

	void const * msg = packetbuf_dataptr();
	unsigned int length = packetbuf_datalen();

	if (is_sink(conn))
	{
		// Pass this messge up to the user
		(*conn->callbacks.recv)(conn, originator);
	}
	else
	{
		// Apply some aggregation function
		if (tree_agg_is_collecting(conn))
		{
			printf("Cont Agg With:%s\n", addr2str(originator));

			(*conn->callbacks.aggregate_update)(conn->data, msg);
		}
		else
		{
			printf("Star Agg Addr:%s\n", addr2str(originator));

			// We need to copy the users data into our memory,
			// So we can apply future aggregtions to it.
			(*conn->callbacks.store_packet)(conn, msg, length);

			// We have started collection
			conn->is_collecting = true;

			// Start aggregation timer
			static struct ctimer aggregate_ct;
			ctimer_set(&aggregate_ct, AGGREGATION_WAIT, &finish_aggregate_collect, conn);
		}
	}
}

static void unicast_sent(struct unicast_conn *c, int status, int num_tx)
{
	printf("unicast sent\n");
}

/** The function that will be executed when a message is received */
static void recv_setup(struct stbroadcast_conn * ptr)
{
	tree_agg_conn_t * conn = conncvt_stbcast(ptr);

	setup_tree_msg_t const * msg = (setup_tree_msg_t const *)packetbuf_dataptr();

	printf("Got setup message from %s\n", addr2str(&msg->source));

	// If the sink received a setup message, then do nothing
	// it doesn't need a parent as it is the root.
	if (is_sink(conn))
	{
		printf("We are the sink node, so should not listen for parents.\n");
		return;
	}

	// If this is the first setup message that we have seen
	// Then we need to start the collect timeout
	if (!conn->has_seen_setup)
	{
		conn->has_seen_setup = true;

		// Indicate that we are setting up
		leds_on(LEDS_RED);

		// Start the timer that will call a function when we are
		// done detecting parents.
		static struct ctimer ct;
		ctimer_set(&ct, PARENT_DETECT_WAIT, &parent_detect_finished, conn);

		printf("Not seen setup message before, so setting timer...\n");
	}

	// As we have received a message we need to record the node
	// it came from, if it is closer to the sink.
	if (msg->hop_count < conn->collecting_best_hop)
	{
		printf("Updating to a better parent (%s H:%u) was:(%s H:%u)\n",
			addr2str(&msg->source), msg->hop_count,
			addr2str(&conn->collecting_best_parent), conn->collecting_best_hop
		);

		// Set the best parent, and the hop count of that node
		rimeaddr_copy(&conn->collecting_best_parent, &msg->source);
		conn->collecting_best_hop = msg->hop_count;
	}

	
	// If the parent of the node that sent this message is this node,
	// then we are not a leaf
	if (conn->is_leaf_node && rimeaddr_cmp(&msg->parent, &rimeaddr_node_addr) != 0)
	{
		printf("Node (%s) is our child, we are not a leaf.\n",
			addr2str(&msg->source));

		conn->is_leaf_node = false;
	}
}

static void sent_stbroadcast(struct stbroadcast_conn * c) { }


static const struct stbroadcast_callbacks callbacks_setup =
	{ &recv_setup, &sent_stbroadcast };

static const struct unicast_callbacks callbacks_aggregate =
	{ &recv_aggregate, &unicast_sent };


void tree_agg_setup_wait_finished(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	leds_on(LEDS_YELLOW);

	// Send the first message that will be used to set up the
	// aggregation tree
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	debug_packet_size(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * msg = (setup_tree_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(setup_tree_msg_t));

	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->parent, &rimeaddr_null);
	msg->hop_count = 0;

	stbroadcast_send_stubborn(&conn->bc, STUBBORN_INTERVAL);

	printf("IsSink, sending initial message...\n");

	// Wait for a bit to allow a few messages to be sent
	static struct ctimer ct;
	ctimer_set(&ct, STUBBORN_WAIT, &stbroadcast_cancel_void, conn);
}



bool tree_agg_open(tree_agg_conn_t * conn, rimeaddr_t const * sink,
                   uint16_t ch1, uint16_t ch2,
                   size_t data_size,
                   tree_agg_callbacks_t const * callbacks)
{
	if (conn != NULL && sink != NULL && callbacks != NULL &&
		callbacks->recv != NULL && callbacks->setup_complete != NULL &&
		callbacks->aggregate_update != NULL && callbacks->aggregate_own != NULL &&
		callbacks->store_packet != NULL)
	{
		stbroadcast_open(&conn->bc, ch1, &callbacks_setup);
		unicast_open(&conn->uc, ch2, &callbacks_aggregate);

		conn->has_seen_setup = false;
		conn->is_collecting = false;
		conn->is_leaf_node = true;

		rimeaddr_copy(&conn->best_parent, &rimeaddr_null);
		rimeaddr_copy(&conn->collecting_best_parent, &rimeaddr_null);

		rimeaddr_copy(&conn->sink, sink);

		conn->best_hop = UINT_MAX;
		conn->collecting_best_hop = UINT_MAX;

		conn->data = malloc(data_size);

		// Make sure memory allocation was successful
		if (conn->data == NULL)
		{
			return false;
		}

		conn->data_length = data_size;

		memcpy(&conn->callbacks, callbacks, sizeof(tree_agg_callbacks_t));

		if (is_sink(conn))
		{
			printf("Starting aggregation tree setup...\n");

			// Wait a bit to allow processes to start up
			static struct ctimer ct;
			ctimer_set(&ct, 10 * CLOCK_SECOND, &tree_agg_setup_wait_finished, conn);
		}

		return true;
	}
	
	return false;
}

void tree_agg_close(tree_agg_conn_t * conn)
{
	if (conn != NULL)
	{
		stbroadcast_close(&conn->bc);
		unicast_close(&conn->uc);

		if (conn->data != NULL)
		{
			free(conn->data);
			conn->data = NULL;
		}
	}
}

void tree_agg_send(tree_agg_conn_t * conn)
{
	if (conn != NULL)
	{
		unicast_send(&conn->uc, &conn->best_parent);
	}
}

bool tree_agg_is_leaf(tree_agg_conn_t const * conn)
{
	return conn != NULL && conn->is_leaf_node;
}

bool tree_agg_is_collecting(tree_agg_conn_t const * conn)
{
	return conn != NULL && conn->is_collecting;
}

/********************************************
 ********* APPLICATION BEGINS HERE **********
 *******************************************/


typedef struct
{
	double temperature;
	double humidity;
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
	collect_msg_t * our_data = (collect_msg_t *)data;
	collect_msg_t const * data_to_apply = (collect_msg_t const *)to_apply;

	our_data->temperature += data_to_apply->temperature;
	our_data->humidity += data_to_apply->humidity;

	our_data->temperature /= 2.0;
	our_data->humidity /= 2.0;
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

	tree_aggregate_update(ptr, &data);
}

static void tree_agg_store_packet(tree_agg_conn_t * conn, void const * packet, unsigned int length)
{
	memcpy(conn->data, packet, length);
}

static void tree_agg_write_data_to_packet(tree_agg_conn_t * conn)
{
	packetbuf_clear();
	packetbuf_set_datalen(conn->data_length);
	debug_packet_size(conn->data_length);
	memcpy(packetbuf_dataptr(), conn->data, conn->data_length);
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

