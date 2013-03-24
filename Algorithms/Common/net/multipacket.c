#include "net/multipacket.h"

#include "sys/clock.h"

#include "debug-helper.h"

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

#define MAX_REXMITS 4
#define SEND_PERIOD ((clock_time_t) 1 * CLOCK_SECOND)

typedef struct
{
	uint16_t id;
	rimeaddr_t target;
	rimeaddr_t source;

	void * data;
	unsigned int length;

	unsigned int sent;
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
	unsigned int length;

	unsigned int data_received;

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

	// Check that we have something to send onwards and
	// that runicast is not currently sending
	if (!linked_list_is_empty(&conn->sending_packets) && !runicast_is_transmitting(&conn->rc))
	{
		multipacket_sending_packet_t * details = linked_list_peek(&conn->sending_packets);

		unsigned int to_send = min(PACKETBUF_SIZE, details->length - details->sent);

		void * send_start = (char *)(details->data) + details->sent;

		packetbuf_clear();
		packetbuf_set_datalen(to_send);
		//debug_packet_size(to_send);
		void * msg = packetbuf_dataptr();
		memcpy(msg, send_start, to_send);

		// Set the id of this packet
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, details->id);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_SEQNO, details->seqno);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ELENGTH, details->length);

		packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &details->source);

		MPDPRINTF("multipacket: Sending a packet of sublength:%d/%d to %s with id %d and seqno %d\n",
			to_send, details->length, addr2str(&details->target), details->id, details->seqno);
		
		runicast_send(&conn->rc, &details->target, MAX_REXMITS);

		// Record the number of bytes we have sent
		details->sent += to_send;
		details->seqno += 1;

		// Check to see if we have finished sending
		if (details->sent == details->length)
		{
			MPDPRINTF("multipacket: Finished sending a packet of length:%d to %s with id %d\n",
				details->length, addr2str(&details->target), details->id);

			conn->callbacks->sent(conn, &details->target, details->data, details->length);

			linked_list_pop(&conn->sending_packets);
		}
	}

	ctimer_reset(&conn->ct_sender);
}

static void recv_from_runicast(struct runicast_conn * rc, rimeaddr_t const * from, uint8_t seqno)
{
	multipacket_conn_t * conn = runicast_conncvt(rc);

	// We have received a packet, now we need to join stuff back together
	uint16_t packet_id = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	uint8_t seq = packetbuf_attr(PACKETBUF_ATTR_EPACKET_SEQNO);
	unsigned int data_length = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ELENGTH);

	rimeaddr_t const * source = packetbuf_addr(PACKETBUF_ADDR_ESENDER);

	void * data_recv = packetbuf_dataptr();
	unsigned int recv_length = packetbuf_datalen();

	recv_key_t key;
	key.data.id = packet_id;
	rimeaddr_copy(&key.data.originator, source);

	multipacket_recieving_packet_t * details = (multipacket_recieving_packet_t *)map_get(&conn->receiving_packets, &key);


	void * data_to_pass_onwards = NULL;
	unsigned int length_of_data_to_pass_onwards = 0;
	bool should_remove_from_map = false;

	if (details == NULL)
	{
		if (seq == 0)
		{
			// We have not received this message before!
			MPDPRINTF("multipacket: Recv'd a new packet from %s with id %d and length %d/%d\n",
				addr2str(from), packet_id, recv_length, data_length);

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
	}
	else
	{
		// Check that this is the next packet that we want
		if (seq == details->last_seqno + 1)
		{
			MPDPRINTF("multipacket: Recv'd a new part of a packet from %s with id %d seqno:%d and length %d/%d\n",
				addr2str(from), packet_id, seq, recv_length, details->length);

			void * data_ptr = (char *)(details->data) + details->data_received;

			memcpy(data_ptr, data_recv, recv_length);

			// Update the data received and the last seqno
			details->data_received += recv_length;
			details->last_seqno = seq;

			// Check if we have got everything
			// If so set the relevant variables
			if (details->data_received == details->length)
			{
				data_to_pass_onwards = details->data;
				length_of_data_to_pass_onwards = details->length;
				should_remove_from_map = true;
			}
		}
	}

	// Check to see if we have fully received this packet
	if (data_to_pass_onwards != NULL)
	{
		MPDPRINTF("multipacket: delivering packet from %s with id %d and length %d\n",
			addr2str(from), packet_id, length_of_data_to_pass_onwards);

		conn->callbacks->recv(conn, source, data_to_pass_onwards, length_of_data_to_pass_onwards);

		if (should_remove_from_map)
		{
			// This packet has been received so remove it
			map_remove(&conn->receiving_packets, &key);
		}
	}
}

static const struct runicast_callbacks rccallbacks = {&recv_from_runicast, NULL, NULL};

bool multipacket_open(multipacket_conn_t * conn, uint16_t channel, multipacket_callbacks_t const * callbacks)
{
	if (conn != NULL)
	{
		runicast_open(&conn->rc, channel, &rccallbacks);
		channel_set_attributes(channel, multipacket_attributes);

		conn->id = 0;

		conn->callbacks = callbacks;

		ctimer_set(&conn->ct_sender, SEND_PERIOD, &send_loop_callback, conn);

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

		linked_list_free(&conn->sending_packets);
		map_free(&conn->receiving_packets);
	}
}

void multipacket_send(multipacket_conn_t * conn, rimeaddr_t const * target, void * data, unsigned int length)
{
	// Allocate the packet
	multipacket_sending_packet_t * details = (multipacket_sending_packet_t *)malloc(sizeof(multipacket_sending_packet_t));
	details->id = conn->id++;
	details->data = data;
	details->length = length;
	details->sent = 0;
	details->seqno = 0;
	rimeaddr_copy(&details->target, target);
	rimeaddr_copy(&details->source, &rimeaddr_node_addr);

	// Add to the queue to send
	linked_list_append(&conn->sending_packets, details);

	MPDPRINTF("multipacket: Adding data of length %d to send to %s with id %d. %u packets queued.\n",
		length, addr2str(target), details->id, linked_list_length(&conn->sending_packets));
}
