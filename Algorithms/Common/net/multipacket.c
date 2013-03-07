#include "net/multipacket.h"

#include "sys/clock.h"

#include "debug-helper.h"

#include <stdlib.h>

#define min(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

static const uint8_t MAX_REXMITS = 4;

typedef struct
{
	uint16_t id;
	rimeaddr_t target;

	void * data;
	size_t length;

	size_t sent;
	uint8_t seqno;

} multipacket_sending_packet_t;

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
	recv_key_t key;

	void * data;
	size_t length;

	size_t data_received;

	uint8_t last_seqno;

} multipacket_recieving_packet_t;

static void sending_packet_cleanup(void * ptr)
{
	multipacket_sending_packet_t * details = (multipacket_sending_packet_t *)ptr;

	free(details->data);
	free(details);
}

static void recieving_packet_cleanup(void * ptr)
{
	multipacket_recieving_packet_t * details = (multipacket_recieving_packet_t *)ptr;

	free(details->data);
	free(details);
}

static bool recv_key_equality(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	recv_key_t const * l = (recv_key_t const *)left;
	recv_key_t const * r = (recv_key_t const *)right;

	return l->i32 == r->i32;
}

static const struct packetbuf_attrlist multipacket_attributes[] = {
	{ PACKETBUF_ATTR_EPACKET_ID, PACKETBUF_ATTR_BYTE * sizeof(uint16_t) },	// ID
	{ PACKETBUF_ATTR_EPACKET_TYPE, PACKETBUF_ATTR_BYTE * sizeof(uint8_t) },	// seqno
	{ PACKETBUF_ATTR_TTL, PACKETBUF_ATTR_BYTE * sizeof(size_t) },	// Length
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

	// Check that we have something to send onwards and
	// that runicast is not currently sending
	if (linked_list_length(&conn->sending_packets) != 0 && !runicast_is_transmitting(&conn->rc))
	{
		multipacket_sending_packet_t * details = linked_list_peek(&conn->sending_packets);

		size_t to_send = min(PACKETBUF_SIZE, details->length - details->sent);

		void * send_start = (char *)(details->data) + details->sent;

		packetbuf_clear();
		packetbuf_set_datalen(to_send);
		debug_packet_size(to_send);
		void * msg = packetbuf_dataptr();
		memcpy(msg, send_start, to_send);

		// Set the id of this packet
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, details->id);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, details->seqno);
		packetbuf_set_attr(PACKETBUF_ATTR_TTL, details->length);
		
		runicast_send(&conn->rc, &details->target, MAX_REXMITS);

		// Record the number of bytes we have sent
		details->sent += to_send;
		details->seqno += 1;

		// Check to see if we have finished sending
		if (details->sent == details->length)
		{
			conn->callbacks->sent(conn, &details->target, details->data, details->length);

			linked_list_pop(&conn->sending_packets);
		}
	}

	ctimer_set(&conn->ct_sender, 3 * CLOCK_SECOND, &send_loop_callback, conn);
}

static void recv_from_runicast(struct runicast_conn * rc, rimeaddr_t * from, uint8_t seqno)
{
	multipacket_conn_t * conn = runicast_conncvt(rc);

	// We have received a packet, now we need to join stuff back together
	uint16_t packet_id = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	uint8_t seq = packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE);
	size_t data_length = packetbuf_attr(PACKETBUF_ATTR_TTL);

	void * data_recv = packetbuf_dataptr();
	size_t recv_length = packetbuf_datalen();

	recv_key_t key;
	key.data.id = packet_id;
	rimeaddr_copy(&key.data.originator, from);

	multipacket_recieving_packet_t * details = (multipacket_recieving_packet_t *)map_get(&conn->receiving_packets, &key);

	if (details == NULL)
	{
		if (seq == 0)
		{
			// We have not received this message before!

			details = (multipacket_recieving_packet_t *)malloc(sizeof(multipacket_recieving_packet_t));
			details->key = key;
			details->data = malloc(data_length);
			details->length = data_length;
			details->last_seqno = seq;
			details->data_received = recv_length;

			memcpy(details->data, data_recv, recv_length);

			map_put(&conn->receiving_packets, details);
		}
	}
	else
	{
		// Check that this is the next packet that we want
		if (seq == details->last_seqno + 1)
		{
			void * data_ptr = (char *)(details->data) + details->data_received;

			memcpy(data_ptr, data_recv, recv_length);

			// Update the data received and the last seqno
			details->data_received += recv_length;
			details->last_seqno = seq;
		}
	}

	// Check to see if we have fully received this packet
	if (details != NULL && details->data_received == details->length)
	{
		conn->callbacks->recv(conn, from, details->data, details->length);

		// This packet has been received so remove it
		map_remove(&conn->receiving_packets, &key);
	}
}

static void sent_by_runicast(struct runicast_conn * rc, rimeaddr_t * to, uint8_t retransmissions)
{
}

static void runicast_timedout(struct runicast_conn * rc, rimeaddr_t * to, uint8_t retransmissions)
{
}

static const struct runicast_callbacks rccallbacks = {&recv_from_runicast, &sent_by_runicast, &runicast_timedout};

bool multipacket_open(multipacket_conn_t * conn, uint16_t channel, multipacket_callbacks_t const * callbacks)
{
	if (conn != NULL)
	{
		runicast_open(&conn->rc, channel, &rccallbacks);
		channel_set_attributes(channel, multipacket_attributes);

		conn->id = 0;

		conn->callbacks = callbacks;

		ctimer_set(&conn->ct_sender, 3 * CLOCK_SECOND, &send_loop_callback, conn);

		linked_list_init(&conn->sending_packets, &sending_packet_cleanup);

		map_init(&conn->receiving_packets, &recv_key_equality, &recieving_packet_cleanup);

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

		linked_list_clear(&conn->sending_packets);
		map_clear(&conn->receiving_packets);
	}
}

void multipacket_send(multipacket_conn_t * conn, rimeaddr_t const * target, void * data, size_t length)
{
	// Allocate the packet
	multipacket_sending_packet_t * details = (multipacket_sending_packet_t *)malloc(sizeof(multipacket_sending_packet_t));
	details->id = conn->id++;
	details->data = data;
	details->length = length;
	details->sent = 0;
	details->seqno = 0;
	rimeaddr_copy(&details->target, target);

	// Add to the queue to send
	linked_list_append(&conn->sending_packets, details);
}
