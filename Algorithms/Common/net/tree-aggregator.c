#include "net/tree-aggregator.h"

#include "contiki.h"
#include "lib/random.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "random-range.h"
#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"

#ifdef TREE_AGG_DEBUG
#	define TADPRINTF(...) printf(__VA_ARGS__)
#else
#	define TADPRINTF(...)
#endif

static inline tree_agg_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (tree_agg_conn_t *)conn;
}

static inline tree_agg_conn_t * conncvt_multipacket(struct multipacket_conn * conn)
{
	return (tree_agg_conn_t *)
		(((char *)conn) - sizeof(struct stbroadcast_conn));
}

// The amount of subborn broadcasts to allow time for and how often for them to broadcast
#define STUBBORN_WAIT_COUNT 3u
#define MIN_SEND_TIME 1
#define MAX_SEND_TIME 3

// Time to gather aggregations over
#define AGGREGATION_WAIT (clock_time_t)(30 * CLOCK_SECOND)

// Time to wait to detect parents
#define PARENT_DETECT_WAIT (clock_time_t)((MAX_SEND_TIME * (1 + STUBBORN_WAIT_COUNT)) * CLOCK_SECOND)

static void stbroadcast_cancel_void_and_callback(void * ptr)
{
	tree_agg_conn_t * conn = conncvt_stbcast((struct stbroadcast_conn *)ptr);

	stbroadcast_cancel(&conn->bc);

	TADPRINTF("Tree Agg: Stubborn bcast canceled, setup complete\n");

	// Start the data generation process
	(*conn->callbacks->setup_complete)(conn);
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

	TADPRINTF("Tree Agg: Timer on %s expired\n", addr2str(&rimeaddr_node_addr));
	TADPRINTF("Tree Agg: Found Parent:%s Hop:%u\n",
		addr2str(&conn->best_parent), conn->best_hop);

	// Send a message that is to be received by the children of this node.
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * msg = (setup_tree_msg_t *)packetbuf_dataptr();

	// We set the parent of this node to be the best parent we heard
	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->parent, &conn->best_parent);
	
	// If at the max, want to keep as UINT_MAX to prevent integer overflow to 0
	msg->hop_count = (conn->best_hop == UINT_MAX) ? UINT_MAX : conn->best_hop + 1;

	clock_time_t send_period = random_time(MIN_SEND_TIME, MAX_SEND_TIME, 0.1);
	clock_time_t wait_period = send_period * STUBBORN_WAIT_COUNT;

	stbroadcast_send_stubborn(&conn->bc, send_period);

	// Wait for a bit to allow a few messages to be sent
	// Then close the connection and tell user that we are done
	ctimer_set(&conn->ct_parent_detect, wait_period, &stbroadcast_cancel_void_and_callback, conn);
}

static void finish_aggregate_collect(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	(*conn->callbacks->aggregate_own)(conn, conn->data);

	void * data;
	size_t length;

	// Copy aggregation data into the packet
	(*conn->callbacks->write_data_to_packet)(conn, &data, &length);

	multipacket_send(&conn->mc, &conn->best_parent, data, length);

	// We need to free the allocated packet data
	free(data);

	TADPRINTF("Tree Agg: Send Agg\n");

	toggle_led_for(LEDS_GREEN, 1 * CLOCK_SECOND);

	// We are no longer collecting aggregation data
	conn->is_collecting = false;

	// Reset the data we have stored to nothing
	// We can do this as write_data_to_packet should have freed the memory
	// if anything was allocated.
	memset(conn->data, 0, conn->data_length);
}

// The function that will be executed when a message is received to be aggregated
static void recv_aggregate(struct multipacket_conn * ptr,
	rimeaddr_t const * originator, void * msg, unsigned int length)
{
	tree_agg_conn_t * conn = conncvt_multipacket(ptr);

	if (tree_agg_is_sink(conn))
	{
		TADPRINTF("Tree Agg: We're sink, got message from %s length:%u, sending to user\n",
			addr2str(originator), length);

		// We need to apply the sink's data to the received data
		(*conn->callbacks->store_packet)(conn, msg, length);
		(*conn->callbacks->aggregate_own)(conn, conn->data);

		void * data;
		size_t data_length;

		// Copy aggregation data into the packet
		(*conn->callbacks->write_data_to_packet)(conn, &data, &data_length);

		// Pass this messge up to the user
		(*conn->callbacks->recv)(conn, originator, data, data_length);

		// Free the allocated data
		free(data);
	}
	else
	{
		// Apply some aggregation function
		if (tree_agg_is_collecting(conn))
		{
			TADPRINTF("Tree Agg: Cont Agg With: %s of length %u\n",
				addr2str(originator), length);

			(*conn->callbacks->aggregate_update)(conn, conn->data, msg, length);
		}
		else
		{
			TADPRINTF("Tree Agg: Start Agg Addr: %s of length %u\n",
				addr2str(originator), length);

			// We need to copy the users data into our memory,
			// So we can apply future aggregtions to it.
			(*conn->callbacks->store_packet)(conn, msg, length);

			// We have started collection
			conn->is_collecting = true;

			// Start aggregation timer
			ctimer_set(&conn->aggregate_ct, AGGREGATION_WAIT, &finish_aggregate_collect, conn);
		}
	}
}

// The function that will be executed when a setup message is received
static void recv_setup(struct stbroadcast_conn * ptr)
{
	toggle_led_for(LEDS_GREEN, CLOCK_SECOND);

	tree_agg_conn_t * conn = conncvt_stbcast(ptr);

	// Store a local copy of the message
	setup_tree_msg_t msg;
	memcpy(&msg, packetbuf_dataptr(), sizeof(setup_tree_msg_t));

	TADPRINTF("Tree Agg: Got setup message from %s\n", addr2str(&msg.source));

	// If the sink received a setup message, then do nothing
	// it doesn't need a parent as it is the root.
	if (tree_agg_is_sink(conn))
	{
		TADPRINTF("Tree Agg: We are the sink node, so should not listen for parents.\n");
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

		TADPRINTF("Tree Agg: Not seen setup message before, so setting timer...\n");
	}

	// As we have received a message we need to record the node
	// it came from, if it is closer to the sink.
	if (msg.hop_count < conn->best_hop)
	{
		// Set the best parent, and the hop count of that node
		rimeaddr_copy(&conn->best_parent, &msg.source);
		conn->best_hop = msg.hop_count;
	}
	
	// If the parent of the node that sent this message is this node,
	// then we are not a leaf
	if (conn->is_leaf_node && rimeaddr_cmp(&msg.parent, &rimeaddr_node_addr))
	{
		TADPRINTF("Tree Agg: Node (%s) is our child, we are not a leaf.\n",
			addr2str(&msg.source));

		conn->is_leaf_node = false;

		leds_off(LEDS_RED);
	}
}

static const struct stbroadcast_callbacks callbacks_setup = { &recv_setup, NULL };
static const struct multipacket_callbacks callbacks_aggregate = { &recv_aggregate, NULL };

void tree_agg_setup_wait_finished(void * ptr)
{
	tree_agg_conn_t * conn = (tree_agg_conn_t *)ptr;

	leds_on(LEDS_BLUE);

	// Send the first message that will be used to set up the aggregation tree
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(setup_tree_msg_t));
	setup_tree_msg_t * msg = (setup_tree_msg_t *)packetbuf_dataptr();

	rimeaddr_copy(&msg->source, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->parent, &rimeaddr_null);
	msg->hop_count = 1;

	clock_time_t send_period = random_time(MIN_SEND_TIME, MAX_SEND_TIME, 0.1);
	clock_time_t wait_period = send_period * STUBBORN_WAIT_COUNT;

	stbroadcast_send_stubborn(&conn->bc, send_period);

	// Wait for a bit to allow a few messages to be sent
	ctimer_set(&conn->ct_wait_finished, wait_period, &stbroadcast_cancel, &conn->bc);
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
		multipacket_open(&conn->mc, ch2, &callbacks_aggregate);

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
			TADPRINTF("Tree Agg: MAF!\n");
			return false;
		}

		conn->data_length = data_size;

		conn->callbacks = callbacks;

		if (tree_agg_is_sink(conn))
		{
			// Wait a bit to allow processes to start up
			ctimer_set(&conn->ct_open,
				10 * CLOCK_SECOND, &tree_agg_setup_wait_finished, conn);
		}

		return true;
	}
	
	return false;
}

void tree_agg_close(tree_agg_conn_t * conn)
{
	TADPRINTF("Tree Agg: Closing connection.\n");

	if (conn != NULL)
	{
		stbroadcast_close(&conn->bc);
		multipacket_close(&conn->mc);

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

void tree_agg_send(tree_agg_conn_t * conn, void * data, size_t length)
{
	if (conn != NULL && data != NULL && length != 0)
	{
		TADPRINTF("Tree Agg: Sending to %s, length=%d\n",
			addr2str(&conn->best_parent), length);
		multipacket_send(&conn->mc, &conn->best_parent, data, length);
	}
}

