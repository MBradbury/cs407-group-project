#include "nhopflood.h"

#include "contiki.h"
#include "dev/leds.h"
#include "net/packetbuf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "net/rimeaddr-helpers.h"
#include "random-range.h"
#include "debug-helper.h"

#ifdef NHOP_FLOOD_DEBUG
#	define NHFDPRINTF(...) printf(__VA_ARGS__)
#else
#	define NHFDPRINTF(...)
#endif

static void nhopflood_delayed_start_sending(void * ptr);

// The custom headers we use
static const struct packetbuf_attrlist flood_attributes[] = {
	{ PACKETBUF_ADDR_ESENDER, PACKETBUF_ADDRSIZE },
	{ PACKETBUF_ATTR_HOPS, PACKETBUF_ATTR_BIT * 4 },
	{ PACKETBUF_ATTR_TTL, PACKETBUF_ATTR_BIT * 4 },
	{ PACKETBUF_ATTR_EPACKET_ID, PACKETBUF_ATTR_BIT * 8 },
	BROADCAST_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};

// Message Structs
typedef struct
{
	rimeaddr_t sender;
	uint8_t id;
	uint8_t ttl;
	uint8_t hops;

	uint8_t retx;

	uint8_t data_length;
	void * data;

} packet_details_t;

static packet_details_t * alloc_packet_details(uint8_t id, uint8_t hops)
{
	packet_details_t * details = (packet_details_t *)malloc(sizeof(packet_details_t));

	rimeaddr_copy(&details->sender, &rimeaddr_node_addr);
	details->id = id;
	details->ttl = hops;
	details->hops = 0;

	details->retx = 0;

	details->data_length = packetbuf_datalen();
	details->data = malloc(details->data_length);

	memcpy(details->data, packetbuf_dataptr(), details->data_length);

	return details;
}

static packet_details_t * packet_details_from_packetbuf(void)
{
	packet_details_t * details = (packet_details_t *)malloc(sizeof(packet_details_t));

	rimeaddr_copy(&details->sender, packetbuf_addr(PACKETBUF_ADDR_ESENDER));
	details->id = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	details->ttl = packetbuf_attr(PACKETBUF_ATTR_TTL);
	details->hops = packetbuf_attr(PACKETBUF_ATTR_HOPS);

	details->retx = 0;

	details->data_length = packetbuf_datalen();
	details->data = malloc(details->data_length);

	memcpy(details->data, packetbuf_dataptr(), details->data_length);

	return details;
}

static void free_packet_details(void * ptr)
{
	packet_details_t * details = (packet_details_t *)ptr;
	if (details != NULL)
	{
		free(details->data);
		free(details);
	}
}


typedef struct
{
	rimeaddr_t from;
	uint8_t id;
	uint8_t hops;
} last_seen_t;


static inline nhopflood_conn_t * conncvt_broadcast(struct broadcast_conn * conn)
{
	return (nhopflood_conn_t *)conn;
}

// We receive a message from a neighbour
static void flood_message_recv(struct broadcast_conn * c, rimeaddr_t const * sender)
{
	// Get a pointer to the nhopflood_conn_t
	nhopflood_conn_t * conn = conncvt_broadcast(c);

	rimeaddr_t const * originator = packetbuf_addr(PACKETBUF_ADDR_ESENDER);
	const uint8_t packet_id = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	const uint8_t hops = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_HOPS);
	const uint8_t ttl = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_TTL);

	// Get the last seen entry for the end-point sender
	last_seen_t * last = map_get(&conn->latest_message_seen, originator);

	bool seenbefore = true;

	// Check if we have seen this packet before
	// Not seen from this node before
	if (last == NULL)
	{
		seenbefore = false;

		// We need to record that we have seen a packet from this sender
		last = (last_seen_t *)malloc(sizeof(last_seen_t));
		rimeaddr_copy(&last->from, originator);
		last->id = packet_id;
		last->hops = hops;

		map_put(&conn->latest_message_seen, last);
	}
	// Not seen this message before, but have received from this node before
	else if (last->id < packet_id || 
			(packet_id == 0 && last->id > 240) // Handle integer overflow
			)
	{
		seenbefore = false;
		last->id = packet_id;
		last->hops = hops;
	}
	// Seen before but this is from a shorter route
	else if (last->id == packet_id && last->hops < hops)
	{
		// Have seen before, but re-deliver
		conn->receive_fn(
			conn,
			originator, 
			hops,
			last->hops
		);

		// We now need to update the hop count we have recorded
		linked_list_elem_t elem;
		for (elem = linked_list_first(&conn->packet_queue);
			linked_list_continue(&conn->packet_queue, elem);
			elem = linked_list_next(elem))
		{
			packet_details_t * data = (packet_details_t *) linked_list_data(&conn->packet_queue, elem);

			if (rimeaddr_cmp(&data->sender, originator) && data->id == last->id)
			{
				const uint8_t hops_diff = data->hops - hops;

				// Update the hops
				data->hops = hops;

				// As we have updated the hops we also need to update the TTL
				data->ttl = (hops_diff > data->ttl) ? 0 : data->ttl - hops_diff;
			}
		}

		last->hops = hops;
	}

	if (!seenbefore)
	{
		conn->receive_fn(
			conn,
			originator, 
			hops,
			0
		);

		// Add this packet to the queue if it needs to be forwarded
		// and we have not seen it before.
		if (ttl != 0)
		{
			//printf("Adding received message to queue\n");
			packet_details_t * details = packet_details_from_packetbuf();
			linked_list_append(&conn->packet_queue, details);
		}
	}
}

// Setup the Stubborn Broadcast Callbacks
static const struct broadcast_callbacks broadcastCallbacks = { &flood_message_recv };



static void nhopflood_delayed_start_sending(void * ptr)
{
	// Get the conn from the pointer provided
	nhopflood_conn_t * conn = (nhopflood_conn_t *)ptr;

	packet_details_t * details = (packet_details_t *) linked_list_peek(&conn->packet_queue);

	if (details != NULL)
	{
		// Only send if the TTL is greater than 0
		if (details->ttl > 0)
		{
			//printf("Sending onwards data with id:%d ttl:%d hops:%d from:%s\n",
			//	details->id, details->ttl - 1, details->hops + 1, addr2str(&details->sender));

			// Create the memory for the packet
			packetbuf_clear();
			packetbuf_set_datalen(details->data_length);
			debug_packet_size(details->data_length);
			void * msg = packetbuf_dataptr();

			// Copy the packet to the buffer
			memcpy(msg, details->data, details->data_length);

			// Set the header flags
			packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &details->sender);
			packetbuf_set_attr(PACKETBUF_ATTR_HOPS, details->hops + 1);
			packetbuf_set_attr(PACKETBUF_ATTR_TTL, details->ttl - 1);
			packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, details->id);

			// Send the packet using normal broadcast
			if (broadcast_send(&conn->bc))
			{
				// Increment the retransmission counter
				details->retx += 1;
			}
		}

		// Remove the current queued packet as we have sent all we intend to send
		// Or the TTL is 0
		if (details->retx >= conn->maxrx || details->ttl == 0)
		{
			//printf("Removing packet from queue RETX(%d >= %d) or TTL(%d == 0)\n", details->retx, conn->maxrx, details->ttl);
			linked_list_pop(&conn->packet_queue);
		}
	}

	// Restart the ctimer
	ctimer_restart(&conn->send_timer);
}


// Initialise n-hop data flooding.
bool nhopflood_start(nhopflood_conn_t * conn, uint8_t ch, nhopflood_recv_fn receive_fn,
	clock_time_t send_period, uint8_t maxrx)
{
	if (conn == NULL || receive_fn == NULL || ch == 0)
	{
		return false;
	}
	
	broadcast_open(&conn->bc, ch, &broadcastCallbacks);
	channel_set_attributes(ch, flood_attributes);

	conn->receive_fn = receive_fn;

	conn->current_id = 0;

	linked_list_init(&conn->packet_queue, &free_packet_details);
	map_init(&conn->latest_message_seen, &rimeaddr_equality, &free);

	conn->send_period = send_period;
	conn->maxrx = maxrx;

	ctimer_set(&conn->send_timer, conn->send_period, &nhopflood_delayed_start_sending, conn);

	return true;
}

// Shutdown n-hop data flooding.
void nhopflood_stop(nhopflood_conn_t * conn)
{
	if (conn != NULL)
	{
		ctimer_stop(&conn->send_timer);

		map_free(&conn->latest_message_seen);
		linked_list_free(&conn->packet_queue);

		broadcast_close(&conn->bc);
	}
}

// Register a request to send this nodes data n hops next round
bool nhopflood_send(nhopflood_conn_t * conn, uint8_t hops)
{
	if (conn == NULL)
	{
		//printf("The nhopflood_conn is null!\n");
		return false;
	}

	// When the number of hops to send to are 0, we can simply
	// do nothing
	if (hops == 0)
	{
		NHFDPRINTF("nhopflood: Nowhere to send data to as hops=0\n");
		return true;
	}

	// Create packet details
	packet_details_t * details = alloc_packet_details(conn->current_id++, hops);

	// Record the details to be sent
	linked_list_append(&conn->packet_queue, details);

	NHFDPRINTF("nhopflood: Added a packet to be sent, now %u packets queued.\n",
		linked_list_length(&conn->packet_queue));

	return true;
}

