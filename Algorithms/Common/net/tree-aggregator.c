#include "net/tree-aggregator.h"

#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "lib/random.h"

#include "random-range.h"
#include "led-helper.h"
#include "sensor-converter.h"
#include "debug-helper.h"


static inline tree_agg_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (tree_agg_conn_t *)conn;
}

static inline tree_agg_conn_t * conncvt_multipacket(struct multipacket_conn * conn)
{
	return (tree_agg_conn_t *)
		(((char *)conn) - sizeof(struct stbroadcast_conn));
}

bool tree_agg_is_sink(tree_agg_conn_t const * conn)
{
	return conn != NULL &&
		rimeaddr_cmp(&conn->sink, &rimeaddr_node_addr);
}


// The maximum number of times the reliable unicast
// will attempt to resend a message.
static const int MAX_RUNICAST_RETX = 5;

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

	void * data;
	size_t length;

	// Copy aggregation data into the packet
	(*conn->callbacks.write_data_to_packet)(conn, &data, &length);

	multipacket_send(&conn->mc, &conn->best_parent, data, length);

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
static void recv_aggregate(struct multipacket_conn * ptr, rimeaddr_t const * originator, void * sent_data, size_t sent_length)
{
	tree_agg_conn_t * conn = conncvt_multipacket(ptr);

	void const * msg = packetbuf_dataptr();
	unsigned int length = packetbuf_datalen();

	if (tree_agg_is_sink(conn))
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

static void multipacket_sent(struct multipacket_conn * c, rimeaddr_t * const to, void * sent_data, size_t sent_length)
{
	printf("Tree Agg: Sent %d bytes to %s\n", sent_length, addr2str(to));
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
	if (tree_agg_is_sink(conn))
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

static const struct multipacket_callbacks callbacks_aggregate =
	{ &recv_aggregate, &multipacket_sent };


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
			printf("Tree Agg: Starting Failed: Memory allocation!\n");
			return false;
		}

		conn->data_length = data_size;

		memcpy(&conn->callbacks, callbacks, sizeof(tree_agg_callbacks_t));

		if (tree_agg_is_sink(conn))
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
	if (conn != NULL && data != NULL)
	{
		printf("Tree Agg: Sending data to best parent %s with length %d\n", addr2str(&conn->best_parent), length);
		multipacket_send(&conn->mc, &conn->best_parent, data, length);
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
