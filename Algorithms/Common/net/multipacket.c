#include "net/multipacket.h"

#include "sys/clock.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef MULTIPACKET_DEBUG
#	define MPDPRINTF(...) printf(__VA_ARGS__)
#else
#	define MPDPRINTF(...)
#endif

// From: http://stackoverflow.com/questions/3437404/min-and-max-in-c
#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
	   __typeof__ (b) _b = (b); \
	 _a < _b ? _a : _b; })

// The maximum number of retransmits before giving up sending a packet
#define MAX_REXMITS 4

// The time between transmits
#define SEND_PERIOD ((clock_time_t) 1 * CLOCK_SECOND)

typedef struct
{
	uint16_t id;
	
	rimeaddr_t target;
	rimeaddr_t source;

	unsigned int length;
	unsigned int sent;
	uint8_t seqno;

	// Data stored from here onwards
} multipacket_sending_packet_t;

// Need to use a union to not break the strict aliasing rule
typedef union
{
	uint32_t i32;

	struct {
		uint16_t id;
		rimeaddr_t originator;
	} data;

} recv_key_t;

typedef struct
{
	recv_key_t key; // Keep this key first
	
	unsigned int length;
	unsigned int data_received;
	uint8_t last_seqno;

	// Data stored from here onwards
} multipacket_receiving_packet_t;

// Gets the pointer to the data stored after the struct
static inline void * sending_data(multipacket_sending_packet_t * packet)
{
	return (packet + 1);
}
static inline void * receiving_data(multipacket_receiving_packet_t * packet)
{
	return (packet + 1);
}

// Equality function for two recv_key_t
static bool recv_key_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	recv_key_t const * l = (recv_key_t const *)left;
	recv_key_t const * r = (recv_key_t const *)right;

	return l->i32 == r->i32;
}

// Repurpose header attributes
#define PACKETBUF_ATTR_EPACKET_SEQNO PACKETBUF_ATTR_EPACKET_TYPE
#define PACKETBUF_ATTR_EPACKET_ELENGTH PACKETBUF_ATTR_TTL

static const struct packetbuf_attrlist multipacket_attributes[] = {
	{ PACKETBUF_ATTR_EPACKET_ID, PACKETBUF_ATTR_BYTE * sizeof(uint16_t) },	// ID
	{ PACKETBUF_ATTR_EPACKET_SEQNO, PACKETBUF_ATTR_BYTE * sizeof(uint8_t) },	// seqno
	{ PACKETBUF_ATTR_EPACKET_ELENGTH, PACKETBUF_ATTR_BYTE * sizeof(unsigned int) },	// Length
	{ PACKETBUF_ADDR_ESENDER, PACKETBUF_ADDRSIZE },
	RUNICAST_ATTRIBUTES
	PACKETBUF_ATTR_LAST
};

static inline multipacket_conn_t * runicast_conncvt(struct runicast_conn * conn)
{
	return (multipacket_conn_t *)conn;
}

static void send_loop_callback(void * ptr)
{
	multipacket_conn_t * conn = (multipacket_conn_t *)ptr;

	// Check that we have something to send onwards and that runicast is not currently sending
	if (!linked_list_is_empty(&conn->sending_packets) && !runicast_is_transmitting(&conn->rc))
	{
		multipacket_sending_packet_t * details = linked_list_peek(&conn->sending_packets);

		unsigned int to_send = min(PACKETBUF_SIZE, details->length - details->sent);

		void * send_start = ((char *) sending_data(details)) + details->sent;

		packetbuf_clear();
		packetbuf_set_datalen(to_send);
		void * msg = packetbuf_dataptr();
		memcpy(msg, send_start, to_send);

		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, details->id);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_SEQNO, details->seqno);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ELENGTH, details->length);
		packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &details->source);

		runicast_send(&conn->rc, &details->target, MAX_REXMITS);

		// Record the number of bytes we have sent
		details->sent += to_send;
		details->seqno += 1;

		// Check to see if we have finished sending
		if (details->sent == details->length)
		{
			conn->callbacks->sent(conn, &details->target, sending_data(details), details->length);

			linked_list_pop(&conn->sending_packets);
		}
	}
	
	// Set the timer to call this function again
	ctimer_reset(&conn->ct_sender);
}

static void recv_from_runicast(struct runicast_conn * rc, rimeaddr_t const * from, uint8_t seqno)
{
	multipacket_conn_t * conn = runicast_conncvt(rc);

	// We have received a packet, now we need to join segmented data back together
	
	const uint16_t packet_id = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	const uint8_t seq = packetbuf_attr(PACKETBUF_ATTR_EPACKET_SEQNO);
	const unsigned int data_length = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ELENGTH);

	rimeaddr_t const * source = packetbuf_addr(PACKETBUF_ADDR_ESENDER);

	void const * data_recv = packetbuf_dataptr();
	const unsigned int recv_length = packetbuf_datalen();

	recv_key_t key;
	key.data.id = packet_id;
	rimeaddr_copy(&key.data.originator, source);

	// Get the data already received about this packet
	multipacket_receiving_packet_t * details =
		(multipacket_receiving_packet_t *)map_get(&conn->receiving_packets, &key);

	void * data_to_pass_onwards = NULL;
	unsigned int length_of_data_to_pass_onwards = 0;
	bool should_remove_from_map = false;

	if (details == NULL)
	{
		// We have not received this message before!
	
		if (seq == 0)
		{
			// OPTIMISATION: avoid allocating memory when we will free it shortly
			// this is the case when we have an entire message in a single packet
			if (recv_length == data_length)
			{
				data_to_pass_onwards = data_recv;
				length_of_data_to_pass_onwards = recv_length;
				// No need to remove from the map as we never added it
			}
			else
			{
				// Record this packet
				details = (multipacket_receiving_packet_t *)
					malloc(sizeof(multipacket_receiving_packet_t) + data_length);
				details->key = key;
				details->length = data_length;
				details->last_seqno = seq;
				details->data_received = recv_length;

				memcpy(receiving_data(details), data_recv, recv_length);

				map_put(&conn->receiving_packets, details);
			}
		}
		
		// If we do not have a record, and the sequency number is greater
		// than 0, then we have already missed the first packet and there
		// would be no point in recording any more.
	}
	else
	{
		// Check that this is the next packet that we want
		if (seq == details->last_seqno + 1)
		{
			void * data_ptr = ((char *) receiving_data(details)) + details->data_received;

			// Copy in the newly receieved data
			memcpy(data_ptr, data_recv, recv_length);

			// Update the data received and the last seqno
			details->data_received += recv_length;
			details->last_seqno = seq;

			// Check if we have got everything, if so set the relevant variables
			if (details->data_received == details->length)
			{
				data_to_pass_onwards = receiving_data(details);
				length_of_data_to_pass_onwards = details->length;
				should_remove_from_map = true;
			}
		}
	}

	// Check to see if we have fully received this packet
	if (data_to_pass_onwards != NULL)
	{
		conn->callbacks->recv(
			conn, source, data_to_pass_onwards, length_of_data_to_pass_onwards);

		if (should_remove_from_map)
		{
			// This packet has been received so remove it
			map_remove(&conn->receiving_packets, &key);
		}
	}
}

static const struct runicast_callbacks rccallbacks = {&recv_from_runicast, NULL, NULL};

bool multipacket_open(multipacket_conn_t * conn,
	uint16_t channel, multipacket_callbacks_t const * callbacks)
{
	if (conn != NULL)
	{
		runicast_open(&conn->rc, channel, &rccallbacks);
		channel_set_attributes(channel, multipacket_attributes);

		conn->id = 0;

		conn->callbacks = callbacks;

		ctimer_set(&conn->ct_sender, SEND_PERIOD, &send_loop_callback, conn);

		linked_list_init(&conn->sending_packets, &free);

		map_init(&conn->receiving_packets, &recv_key_equality, &free);

		return true;
	}

	return false;
}

void multipacket_close(multipacket_conn_t * conn)
{
	if (conn != NULL)
	{
		runicast_close(&conn->rc);

		ctimer_stop(&conn->ct_sender);

		linked_list_free(&conn->sending_packets);
		map_free(&conn->receiving_packets);
	}
}

void multipacket_send(multipacket_conn_t * conn, rimeaddr_t const * target,
	void * data, unsigned int length)
{
	// Allocate the packet to send
	multipacket_sending_packet_t * details =
		(multipacket_sending_packet_t *)
			malloc(sizeof(multipacket_sending_packet_t) + length);
	details->id = conn->id++;
	details->length = length;
	details->sent = 0;
	details->seqno = 0;
	rimeaddr_copy(&details->target, target);
	rimeaddr_copy(&details->source, &rimeaddr_node_addr);

	memcpy(sending_data(details), data, length);

	// Add to the queue to send
	linked_list_append(&conn->sending_packets, details);
}
