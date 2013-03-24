#include "nhopreq.h"

#include "contiki.h"
#include "packetbuf.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lib/random.h"

#include "net/rimeaddr-helpers.h"

#include "random-range.h"
#include "sensor-converter.h"
#include "debug-helper.h"

#ifdef NHOPREQ_DEBUG
#	define NHRDPRINTF(...) printf(__VA_ARGS__)
#else
#	define NHRDPRINTF(...)
#endif

#ifndef STBROADCAST_ATTRIBUTES
#	define STBROADCAST_ATTRIBUTES BROADCAST_ATTRIBUTES
#endif

// The custom headers we use
static const struct packetbuf_attrlist stbroadcast_attributes[] = {
	{ PACKETBUF_ADDR_ESENDER, PACKETBUF_ADDRSIZE },
	{ PACKETBUF_ADDR_SENDER, PACKETBUF_ADDRSIZE },
	{ PACKETBUF_ATTR_HOPS, PACKETBUF_ATTR_BIT * 4 },
	{ PACKETBUF_ATTR_TTL, PACKETBUF_ATTR_BIT * 4 },
	{ PACKETBUF_ATTR_EPACKET_ID, PACKETBUF_ATTR_BIT * 16 },
	STBROADCAST_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};
static const struct packetbuf_attrlist runicast_attributes[] = {
	{ PACKETBUF_ADDR_ESENDER, PACKETBUF_ADDRSIZE },
	{ PACKETBUF_ADDR_ERECEIVER, PACKETBUF_ADDRSIZE },
	{ PACKETBUF_ATTR_HOPS, PACKETBUF_ATTR_BIT * 4 },
	RUNICAST_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};

static const uint8_t RUNICAST_MAX_RETX = 3;
static const clock_time_t STUBBORN_SEND_REPEATS = 3;

typedef struct
{
	// Make sure about is the first entry
	rimeaddr_t about;

	// This how far we belive the node to be, we want
	// the shortest path to this node, so we record how far 
	// messages have taken to come from it to the current node.
	uint8_t hops;

	// The node to wards messages to if you want to send to
	// the about node.
	rimeaddr_t forward_to;

	// The latest id of message that we have seen
	uint16_t id;

} path_record_t;

// Struct used to ask other nodes for predicate values
typedef struct
{
} request_data_msg_t;


static inline nhopreq_conn_t * conncvt_runicast(struct runicast_conn * conn)
{
	return (nhopreq_conn_t *)conn;
}

static inline nhopreq_conn_t * conncvt_datareq_bcast(struct stbroadcast_conn * conn)
{
	return (nhopreq_conn_t *)
		(((char *)conn) - sizeof(struct runicast_conn));
}


// Argument structs
typedef struct
{
	nhopreq_conn_t * conn;
	rimeaddr_t target;
} delayed_reply_data_params_t;

typedef struct
{
	nhopreq_conn_t * conn;
	rimeaddr_t sender;
	rimeaddr_t target;
	uint8_t hops;
} delayed_forward_reply_params_t;


// Prototypes
static void delayed_reply_data(void * ptr);

static bool send_n_hop_data_request(
	nhopreq_conn_t * conn, rimeaddr_t const * originator,
	uint16_t message_id, uint8_t hop_limit, uint8_t hops);

static void send_reply(
	nhopreq_conn_t * conn, rimeaddr_t const * sender,
	rimeaddr_t const * eventual_target, uint8_t hops, void const * data);


// STUBBORN BROADCAST
static void datareq_stbroadcast_recv(struct stbroadcast_conn * c)
{
	nhopreq_conn_t * conn = conncvt_datareq_bcast(c);

#ifdef NHRDPRINTF
	if (packetbuf_datalen() != sizeof(request_data_msg_t))
	{
		printf("nhopreq: Packet length not as expected\n");
	}
#endif

	rimeaddr_t originator, sender;

	rimeaddr_copy(&originator, packetbuf_addr(PACKETBUF_ADDR_ESENDER));
	rimeaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
	const uint16_t message_id = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	const uint8_t hop_limit = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_TTL);
	const uint8_t hops = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_HOPS);

	// We don't want to do anything if the message we sent
	// has got back to ourselves
	if (rimeaddr_cmp(&originator, &rimeaddr_node_addr))
	{
		return;
	}

	printf("nhopreq: I just recieved a Stubborn Broadcast Message! Originator: %s Hop: %u Message ID: %u\n",
		addr2str(&originator), hop_limit, message_id);

	bool respond = false;

	// Check message has not been received before
	path_record_t * record = (path_record_t *)map_get(&conn->mote_records, &originator);

	if (record == NULL)
	{
		printf("nhopreq: Not seen message from %s with id %u before.\n",
			addr2str(&originator), message_id);

		record = (path_record_t *)malloc(sizeof(path_record_t));

		rimeaddr_copy(&record->about, &originator);
		record->hops = hops;
		rimeaddr_copy(&record->forward_to, &sender);
		record->id = message_id;

		map_put(&conn->mote_records, record);

		respond = true;
	}
	else
	{
		// Update path back to originator
		if (hops < record->hops)
		{
			printf("nhopreq: Updating forward path of %s to ", addr2str(&originator));
			printf("%s and %u hops\n", addr2str(&sender), hops);

			record->hops = hops;
			rimeaddr_copy(&record->forward_to, &sender);
		}

		// This is a newer message, so we need to respond to it.
		if (message_id > record->id)
		{
			printf("nhopreq: Seen a newer message from %s (%u), so we will need to respond with our data.\n",
				addr2str(&originator), message_id);

			record->id = message_id;
			respond = true;
		}
	}

	// Respond To
	if (respond)
	{
		// Send predicate value back to originator
		delayed_reply_data_params_t * p =
			(delayed_reply_data_params_t *)
				malloc(sizeof(delayed_reply_data_params_t));

		p->conn = conn;
		rimeaddr_copy(&p->target, &originator);

		// In time we will need to reply to this
		ctimer_set(
			&conn->runicast_timer, 21 * CLOCK_SECOND,
			&delayed_reply_data, p);

		// Forwward request onwards if we have not reached hop limit
		if (hop_limit > 0)
		{
			// Broadcast message onwards
			send_n_hop_data_request(
				conn, &originator, message_id,
				hop_limit - 1, hops + 1);
		}
	}
}

// TODO: need to collate responses, then send the on,
// right now messages collide while a node is trying to forward
// RELIABLE UNICAST
static void runicast_recv(struct runicast_conn * c, rimeaddr_t const * from, uint8_t seqno)
{
	nhopreq_conn_t * conn = conncvt_runicast(c);

	// When receive message, forward the message on to the originator
	// if the final originator, do something with the value

	char tmpBuffer[PACKETBUF_SIZE];
	memcpy(tmpBuffer, packetbuf_dataptr(), packetbuf_datalen());

	rimeaddr_t sender, target;

	rimeaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_ESENDER));
	rimeaddr_copy(&target, packetbuf_addr(PACKETBUF_ADDR_ERECEIVER));
	const uint8_t hops = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_HOPS) + 1;


	void * msgdata = tmpBuffer;
	
	// If this node was the one who sent the message
	if (rimeaddr_cmp(&rimeaddr_node_addr, &target)) 
	{
		// The target node has received the required data,
		// so provide it to the upper layer
		conn->callbacks->receive_fn(conn, &sender, hops, msgdata);
	}
	else
	{
		printf("nhopreq: Trying to forward data to: %s\n", addr2str(&target));

		send_reply(
			conn,
			&sender, // Source
			&target, // Destination
			hops,
			msgdata
		);
	}
}

static void runicast_timedout(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("nhopreq: Runicast timed out to:%s retransmissions:%u\n", addr2str(to), retransmissions);
}


// Callbacks
static const struct runicast_callbacks runicastCallbacks =
	{ &runicast_recv, NULL, &runicast_timedout };

static const struct stbroadcast_callbacks datareq_stbroadcastCallbacks =
	{ &datareq_stbroadcast_recv, NULL };


// Methods
static void delayed_reply_data(void * ptr)
{
	delayed_reply_data_params_t * p =
		(delayed_reply_data_params_t *)ptr;

	printf("nhopreq: Starting delayed send of node data to %s\n", addr2str(&p->target));

	send_reply(
		p->conn,
		&rimeaddr_node_addr, // Source
		&p->target, // Destination
		0,
		NULL
	);

	// Need to free allocated parameter struct
	free(ptr);
}

static void delayed_forward_reply(void * ptr)
{
	delayed_forward_reply_params_t * p =
		(delayed_forward_reply_params_t *)ptr;

	void const * data_dest = (void *)(p + 1);

	send_reply(p->conn, &p->sender, &p->target, p->hops, data_dest);

	// Need to free allocated parameter struct
	free(ptr);
}

static void send_reply(
	nhopreq_conn_t * conn, rimeaddr_t const * sender,
	rimeaddr_t const * eventual_target, uint8_t hops, void const * data)
{
	if (runicast_is_transmitting(&conn->ru))
	{
		printf("nhopreq: runicast is already transmitting, trying again in a few seconds\n");

		delayed_forward_reply_params_t * p =
			(delayed_forward_reply_params_t *)
				malloc(sizeof(delayed_forward_reply_params_t) + conn->data_size);

		p->conn = conn;
		rimeaddr_copy(&p->sender, sender);
		rimeaddr_copy(&p->target, eventual_target);
		p->hops = hops;

		void * data_dest = (void *)(p + 1);

		if (data == NULL)
		{
			// Call data get functions and store result in outwards bound packet
			conn->callbacks->data_fn(conn, data_dest);
		}
		else
		{
			// Copy the provided data
			memcpy(data_dest, data, conn->data_size);
		}

		ctimer_set(&conn->forward_timer, random_time(2, 4, 0.1), &delayed_forward_reply, p);
	}
	else
	{
		printf("nhopreq: Trying to send reply to %s\n", addr2str(eventual_target));

		unsigned int packet_size = conn->data_size;

		packetbuf_clear();
		packetbuf_set_datalen(packet_size);
		debug_packet_size(packet_size);
		void * data_dest = packetbuf_dataptr();
		memset(data_dest, 0, packet_size);

		packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, sender);
		packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, eventual_target);
		packetbuf_set_attr(PACKETBUF_ATTR_HOPS, hops);

		if (data == NULL)
		{
			// Call data get functions and store result in outwards bound packet
			conn->callbacks->data_fn(conn, data_dest);
		}
		else
		{
			// Copy the provided data
			memcpy(data_dest, data, conn->data_size);
		}

		path_record_t * record = (path_record_t *)map_get(&conn->mote_records, eventual_target);

		if (record != NULL)
		{
			runicast_send(&conn->ru, &record->forward_to, RUNICAST_MAX_RETX);
		}
		else
		{
			printf("nhopreq: Failed to find a node to forward the data to %s\n",
				addr2str(eventual_target));
		}
	}
}



static bool send_n_hop_data_request(
	nhopreq_conn_t * conn, rimeaddr_t const * originator,
	uint16_t message_id, uint8_t hop_limit, uint8_t hops)
{
	if (conn == NULL || originator == NULL || hop_limit == 0)
	{
		return false;
	}

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(request_data_msg_t));
	request_data_msg_t * msg = (request_data_msg_t *)packetbuf_dataptr();

	packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, originator);
	packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &rimeaddr_node_addr);
	packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, message_id);
	packetbuf_set_attr(PACKETBUF_ATTR_TTL, hop_limit);
	packetbuf_set_attr(PACKETBUF_ATTR_HOPS, hops);

	// Generate a random number between 2 and 4 to determine how
	// often we send messages
	clock_time_t random_send_time = random_time(2, 4, 0.1);

	clock_time_t send_limit = random_send_time * STUBBORN_SEND_REPEATS;

	printf("nhopreq: Starting sbcast every %lu/%lu second(s) for %lu/%lu seconds\n",
		random_send_time, CLOCK_SECOND, send_limit, CLOCK_SECOND);

	stbroadcast_send_stubborn(&conn->bc, random_send_time);

	ctimer_set(&conn->datareq_stbroadcast_stop_timer, send_limit, &stbroadcast_cancel, &conn->bc);

	return true;
}


// Initialise multi-hop predicate checking
bool nhopreq_start(
	nhopreq_conn_t * conn, uint8_t ch1, uint8_t ch2,
	unsigned int data_size, nhopreq_callbacks_t const * callbacks)
{
	if (conn == NULL || callbacks == NULL ||
		callbacks->data_fn == NULL || ch1 == ch2 || data_size == 0 ||
		callbacks->receive_fn == NULL)
	{
		return false;
	}

	// We need to set the random number generator here
	random_init(*(uint16_t*)(&rimeaddr_node_addr));

	stbroadcast_open(&conn->bc, ch1, &datareq_stbroadcastCallbacks);
	channel_set_attributes(ch1, stbroadcast_attributes);

	runicast_open(&conn->ru, ch2, &runicastCallbacks);
	channel_set_attributes(ch2, runicast_attributes);

	conn->message_id = 1;

	conn->callbacks = callbacks;

	conn->data_size = data_size;

	map_init(&conn->mote_records, &rimeaddr_equality, &free);

	return true;
}

// Shutdown multi-hop predicate checking
bool nhopreq_stop(nhopreq_conn_t * conn)
{
	if (conn == NULL)
	{
		return false;
	}

	ctimer_stop(&conn->runicast_timer);
	ctimer_stop(&conn->forward_timer);
	ctimer_stop(&conn->datareq_stbroadcast_stop_timer);

	runicast_close(&conn->ru);
	stbroadcast_close(&conn->bc);

	// Free List
	map_free(&conn->mote_records);

	return true;
}

void nhopreq_request_info(nhopreq_conn_t * conn, uint8_t hops)
{
	send_n_hop_data_request(conn, &rimeaddr_node_addr, conn->message_id++, hops, 0);
}

