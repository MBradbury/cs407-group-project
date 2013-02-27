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

#include "tree-aggregator.h"


static inline tree_agg_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (tree_agg_conn_t *)conn;
}

static inline tree_agg_conn_t * conncvt_runicast(struct runicast_conn * conn)
{
	return (tree_agg_conn_t *)
		(((char *)conn) - sizeof(struct stbroadcast_conn));
}


static inline bool is_sink(tree_agg_conn_t const * conn)
{
	return conn != NULL &&
		rimeaddr_cmp(&conn->sink, &rimeaddr_node_addr);
}


// The times stubborn broadcasting will use
// to intersperse message resends
static const clock_time_t STUBBORN_WAIT = 60 * CLOCK_SECOND;

// Time to gather aggregations over
static const clock_time_t AGGREGATION_WAIT = 45 * CLOCK_SECOND;

// Time to wait to detect parents
static const clock_time_t PARENT_DETECT_WAIT = 35 * CLOCK_SECOND;

static const uint8_t RUNICAST_MAX_RETX = 6;


static void stbroadcast_cancel_void(void * ptr)
{
	stbroadcast_cancel(&conncvt_stbcast((struct stbroadcast_conn *)ptr)->bc);

	printf("Tree Agg: Stubborn bcast canceled\n");
}

static void stbroadcast_cancel_void_and_callback(void * ptr)
{
	tree_agg_conn_t * conn = conncvt_stbcast((struct stbroadcast_conn *)ptr);

	stbroadcast_cancel(&conn->bc);

	printf("Tree Agg: Stubborn bcast canceled, setup complete\n");

	// Start the data generation process
	(*conn->callbacks.setup_complete)(conn);
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

	printf("Tree Agg: Timer on %s expired\n",
		addr2str(&rimeaddr_node_addr));

	printf("Tree Agg: Found Parent:%s Hop:%u\n",
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
	
	// If at the max, want to keep as UINT_MAX to prevent
	// integer overflow to 0
	if (conn->best_hop == UINT_MAX) 
	{
		msg->hop_count = UINT_MAX;
	}
	else 
	{
		msg->hop_count = conn->best_hop + 1;
	}

	printf("Sending setup message onwards with hop count %u\n", msg->hop_count);

	stbroadcast_send_stubborn(&conn->bc, random_time(2, 4, 0.1));

	// Wait for a bit to allow a few messages to be sent
	// Then close the connection and tell user that we are done
	ctimer_set(&conn->ct_parent_detect, STUBBORN_WAIT, &stbroadcast_cancel_void_and_callback, conn);
}


static void finish_aggregate_collect(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	(*conn->callbacks.aggregate_own)(conn->data);

	// Copy aggregation data into the packet
	(*conn->callbacks.write_data_to_packet)(conn);

	runicast_send(&conn->uc, &conn->best_parent, RUNICAST_MAX_RETX);

	printf("Tree Agg: Send Agg\n");

	toggle_led_for(LEDS_GREEN, 1 * CLOCK_SECOND);

	// We are no longer collecting aggregation data
	conn->is_collecting = false;

	// Reset the data we have stored to nothing
	// We cano do this as write_data_to_packet should have freed the memory
	// if anything was allocated.
	memset(conn->data, 0, conn->data_length);
}

/** The function that will be executed when a message is received */
static void recv_aggregate(struct runicast_conn * ptr, const rimeaddr_t * originator, uint8_t seqno)
{
	tree_agg_conn_t * conn = conncvt_runicast(ptr);

	void const * msg = packetbuf_dataptr();
	unsigned int length = packetbuf_datalen();

	if (is_sink(conn))
	{
		printf("Tree Agg: We're sink, got message, sending to user\n");

		// Pass this messge up to the user
		(*conn->callbacks.recv)(conn, originator);
	}
	else
	{
		// Apply some aggregation function
		if (tree_agg_is_collecting(conn))
		{
			printf("Tree Agg: Cont Agg With: %s\n", addr2str(originator));

			(*conn->callbacks.aggregate_update)(conn->data, msg);
		}
		else
		{
			printf("Tree Agg: Start Agg Addr: %s\n", addr2str(originator));

			// We need to copy the users data into our memory,
			// So we can apply future aggregtions to it.
			(*conn->callbacks.store_packet)(conn, msg, length);

			// We have started collection
			conn->is_collecting = true;

			// Start aggregation timer
			ctimer_set(&conn->aggregate_ct, AGGREGATION_WAIT, &finish_aggregate_collect, conn);
		}
	}
}

static void runicast_sent(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("Tree Agg: runicast sent to %s numtx:%d\n", addr2str(to), retransmissions);
}

static void runicast_timedout(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("Tree Agg: runicast timedout to %s numtx:%d\n", addr2str(to), retransmissions);

	// TODO: If we are getting a lot of these, then it may be the case
	// that our target node has been lost.
	// We may wish to consider other neighbours
}


/** The function that will be executed when a message is received */
static void recv_setup(struct stbroadcast_conn * ptr)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	tree_agg_conn_t * conn = conncvt_stbcast(ptr);

	setup_tree_msg_t const * msg = (setup_tree_msg_t const *)packetbuf_dataptr();

	printf("Tree Agg: Got setup message from %s\n", addr2str(&msg->source));

	// If the sink received a setup message, then do nothing
	// it doesn't need a parent as it is the root.
	if (is_sink(conn))
	{
		printf("Tree Agg: We are the sink node, so should not listen for parents.\n");
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
		ctimer_set(&conn->ctrecv, PARENT_DETECT_WAIT, &parent_detect_finished, conn);

		printf("Tree Agg: Not seen setup message before, so setting timer...\n");
	}

	// As we have received a message we need to record the node
	// it came from, if it is closer to the sink.
	if (msg->hop_count < conn->best_hop)
	{
		char firstaddr[RIMEADDR_STRING_LENGTH];
		char secondaddr[RIMEADDR_STRING_LENGTH];

		printf("Tree Agg: Updating to a better parent (%s H:%u) was:(%s H:%u)\n",
			addr2str_r(&msg->source, firstaddr, RIMEADDR_STRING_LENGTH), msg->hop_count,
			addr2str_r(&conn->best_parent, secondaddr, RIMEADDR_STRING_LENGTH), conn->best_hop
		);

		// Set the best parent, and the hop count of that node
		rimeaddr_copy(&conn->best_parent, &msg->source);
		conn->best_hop = msg->hop_count;
	}
	else
	{
		char firstaddr[RIMEADDR_STRING_LENGTH];
		char secondaddr[RIMEADDR_STRING_LENGTH];

		printf("Tree Agg: Ignoring worse (or equal) parent (%s H:%u) currently:(%s H:%u)\n",
			addr2str_r(&msg->source, firstaddr, RIMEADDR_STRING_LENGTH), msg->hop_count,
			addr2str_r(&conn->best_parent, secondaddr, RIMEADDR_STRING_LENGTH), conn->best_hop
		);
	}

	
	// If the parent of the node that sent this message is this node,
	// then we are not a leaf
	if (conn->is_leaf_node && rimeaddr_cmp(&msg->parent, &rimeaddr_node_addr))
	{
		printf("Tree Agg: Node (%s) is our child, we are not a leaf.\n",
			addr2str(&msg->source));

		conn->is_leaf_node = false;
	}
}

static void sent_stbroadcast(struct stbroadcast_conn * c) { }


static const struct stbroadcast_callbacks callbacks_setup =
	{ &recv_setup, &sent_stbroadcast };

static const struct runicast_callbacks callbacks_aggregate =
	{ &recv_aggregate, &runicast_sent, &runicast_timedout };


void tree_agg_setup_wait_finished(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	leds_on(LEDS_BLUE);

	// Send the first message that will be used to set up the
	// aggregation tree
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	debug_packet_size(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * msg = (setup_tree_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(setup_tree_msg_t));

	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->parent, &rimeaddr_null);
	msg->hop_count = 1u;

	stbroadcast_send_stubborn(&conn->bc, random_time(2, 4, 0.1));

	printf("Tree Agg: IsSink, sending initial message...\n");

	// Wait for a bit to allow a few messages to be sent
	ctimer_set(&conn->ct_wait_finished, STUBBORN_WAIT, &stbroadcast_cancel_void, conn);
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
		printf("Tree Agg: Starting... %p\n",conn);

		stbroadcast_open(&conn->bc, ch1, &callbacks_setup);

		runicast_open(&conn->uc, ch2, &callbacks_aggregate);

		conn->has_seen_setup = false;
		conn->is_collecting = false;
		conn->is_leaf_node = true;

		rimeaddr_copy(&conn->best_parent, &rimeaddr_null);

		rimeaddr_copy(&conn->sink, sink);

		conn->best_hop = UINT_MAX;

		conn->data = malloc(data_size);

		// Make sure memory allocation was successful
		if (conn->data == NULL)
		{
			printf("Tree Agg: Starting Failed: Memory allocation!\n");
			return false;
		}

		conn->data_length = data_size;

		memcpy(&conn->callbacks, callbacks, sizeof(tree_agg_callbacks_t));

		if (is_sink(conn))
		{
			printf("Tree Agg: Starting aggregation tree setup...\n");

			// Wait a bit to allow processes to start up
			ctimer_set(&conn->ct_open, 10 * CLOCK_SECOND, &tree_agg_setup_wait_finished, conn);
		}

		printf("Tree Agg: Starting Succeeded!\n");

		return true;
	}
	
	printf("Tree Agg: Starting Failed: Parameters Invalid!\n");
	return false;
}

void tree_agg_close(tree_agg_conn_t * conn)
{
	printf("Tree Agg: Closing connection.\n");

	if (conn != NULL)
	{
		stbroadcast_close(&conn->bc);
		runicast_close(&conn->uc);

		if (conn->data != NULL)
		{
			free(conn->data);
			conn->data = NULL;
		}

		ctimer_stop(&conn->ctrecv);
		ctimer_stop(&conn->aggregate_ct);
		ctimer_stop(&conn->ct_parent_detect);
		ctimer_stop(&conn->ct_open);
		ctimer_stop(&conn->ct_wait_finished);
	}
}

void tree_agg_send(tree_agg_conn_t * conn)
{
	if (conn != NULL)
	{
		printf("Tree Agg: Sending data to best parent %s\n", addr2str(&conn->best_parent));

		runicast_send(&conn->uc, &conn->best_parent, RUNICAST_MAX_RETX);
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

#ifdef BUILDING_TREE_AGGREGATION_APP

/********************************************
 ********* APPLICATION BEGINS HERE **********
 *******************************************/


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


static void tree_agg_recv(tree_agg_conn_t * conn, rimeaddr_t const * source)
{
	collect_msg_t const * msg = (collect_msg_t const *)packetbuf_dataptr();

	printf("Tree Agg: Sink rcv: Src:%s Temp:%d Hudmid:%d%% Light1:%d Light2:%d\n",
			addr2str(source),
			(int)msg->temperature, (int)msg->humidity, (int)msg->light1, (int)msg->light2
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

		// Create the data message that we are going to send
		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		debug_packet_size(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
		memset(msg, 0, sizeof(collect_msg_t));

		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);
		msg->light1 = s1087_light1(raw_light1);
		msg->light2 = s1087_light1(raw_light2);
		
		tree_agg_send(&conn);

		etimer_reset(&et);

		leds_off(LEDS_BLUE);
	}

	PROCESS_END();
}

#endif


