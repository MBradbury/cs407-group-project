#include "nhopreq.h"

#include "contiki.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lib/random.h"

#include "sensor-converter.h"
#include "debug-helper.h"

// Struct for the list elements, used to see if messages have already been sent
typedef struct
{
	rimeaddr_t originator;
	uint8_t message_id;
	uint8_t hops;
} list_elem_t;

// Struct used to ask other nodes for predicate values
typedef struct
{
	rimeaddr_t originator;
	uint8_t message_id;
	uint8_t hop_limit;
} req_data_msg_t;

// Struct to send back to the originator with the value of a predicate
typedef struct
{
	rimeaddr_t sender;
	rimeaddr_t target_receiver;
	uint8_t message_id;
	// After this point the user generated data will be contained
} return_data_msg_t;


static inline nhopreq_conn_t * conncvt_runicast(struct runicast_conn * conn)
{
	return (nhopreq_conn_t *)conn;
}

static inline nhopreq_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (nhopreq_conn_t *)
		(((char *)conn) - sizeof(struct runicast_conn));
}


// Argument structs
typedef struct
{
	nhopreq_conn_t * conn;
	uint8_t message_id;
} delayed_send_evaluated_predicate_params_t;

typedef struct
{
	nhopreq_conn_t * conn;
	return_data_msg_t msg;
} delayed_forward_evaluated_predicate_params_t;


// Prototypes
static void delayed_send_evaluated_predicate(void * ptr);

static void send_n_hop_predicate_check(
	nhopreq_conn_t * conn, rimeaddr_t const *originator,
	uint8_t message_id_to_send, uint8_t hop_limit);

static void send_evaluated_predicate(
	nhopreq_conn_t * hc, rimeaddr_t const * sender, rimeaddr_t const * target_receiver,
	uint8_t message_id, void const * data);

static uint8_t get_message_id(nhopreq_conn_t * conn);

// STUBBORN BROADCAST
static void stbroadcast_recv(struct stbroadcast_conn * c)
{
	nhopreq_conn_t * hc = conncvt_stbcast(c);

	// Copy Packet Buffer To Memory
	char tmpBuffer[PACKETBUF_SIZE];
	packetbuf_copyto(tmpBuffer);
	req_data_msg_t * msg = (req_data_msg_t *)tmpBuffer;

	//  printf("I just recieved a Stubborn Broadcast Message! Originator: %s Message: %s Hop: %d Message ID: %d\n",
	//	  addr2str(&msg->originator),
	//	  msg->predicate_to_check,
	//	  msg->hop_limit,
	//	  msg->message_id);

	// Check message has not been received before
	bool deliver_msg = false;

	array_list_elem_t elem;
	for (elem = array_list_first(&hc->message_list); array_list_continue(&hc->message_list, elem); elem = array_list_next(elem))
	{
		list_elem_t * data = (list_elem_t *)array_list_data(&hc->message_list, elem);

		// Message has been delivered before
		if (data->message_id == msg->message_id)
		{
			// If the new message has a higher hop count
			if (msg->hop_limit > data->hops)
			{
				printf("Message received before and hops is higher\n");

				// Update the new originator and hop count
				rimeaddr_copy(&data->originator, &msg->originator);

				data->hops = msg->hop_limit;

				deliver_msg = true;
			}
			break;
		}
	}	

	// End of List and the Message has NOT been delivered before
	if (!array_list_continue(&hc->message_list, elem))
	{
		list_elem_t * delivered_msg = (list_elem_t *)malloc(sizeof(list_elem_t));

		rimeaddr_copy(&delivered_msg->originator, &msg->originator);
		delivered_msg->message_id = msg->message_id;
		delivered_msg->hops = msg->hop_limit;

		array_list_append(&hc->message_list, delivered_msg);

		deliver_msg = true;
	}

	// Respond To
	if (deliver_msg)
	{
		// Send predicate value back to originator
		delayed_send_evaluated_predicate_params_t * p =
			(delayed_send_evaluated_predicate_params_t *)
				malloc(sizeof(delayed_send_evaluated_predicate_params_t));

		p->conn = hc;
		p->message_id = msg->message_id;

		static struct ctimer runicast_timer;
		ctimer_set(&runicast_timer, 21 * CLOCK_SECOND,
			&delayed_send_evaluated_predicate, p);

		// Rebroadcast Message If Hop Count Is Greater Than 1
		if (msg->hop_limit > 1) // last node
		{
			// Broadcast Message On
			send_n_hop_predicate_check(
				hc, &rimeaddr_node_addr,
				msg->message_id, msg->hop_limit - 1);
		}
	}
}

static void stbroadcast_sent(struct stbroadcast_conn *c)
{
	//printf("I've sent!\n");
}

static void stbroadcast_callback_cancel(void *ptr)
{
	nhopreq_conn_t * conn = (nhopreq_conn_t *)ptr;

	printf("Canceling Stubborn Broadcast.\n");
	stbroadcast_cancel(&conn->bc);
}

// TODO: need to collate responses, then send the on,
// right now messages collide while a node is trying to forward
// RELIABLE UNICAST
static void runicast_recv(struct runicast_conn * c, rimeaddr_t const * from, uint8_t seqno)
{
	nhopreq_conn_t * conn = conncvt_runicast(c);

	//printf("runicast received from %s\n", addr2str(from));

	// When recieve message, forward the message on to the originator
	// if the final originator, do something with the value

	// Copy Packet Buffer To Memory
	char tmpBuffer[PACKETBUF_SIZE];
	packetbuf_copyto(tmpBuffer);
	return_data_msg_t * msg = (return_data_msg_t *)tmpBuffer;

	array_list_elem_t elem;
	for (elem = array_list_first(&conn->message_list); array_list_continue(&conn->message_list, elem); elem = array_list_next(elem))
	{
		list_elem_t * data = (list_elem_t *)array_list_data(&conn->message_list, elem);
	
		if (data->message_id == msg->message_id)
		{
			// If this node was the one who sent the message
			if (rimeaddr_cmp(&rimeaddr_node_addr, &data->originator)) 
			{
				// The target node has received the required data,
				// so provide it to the upper layer
				(*conn->receive_fn)(
					&msg->sender, data->hops,
					((char *)msg) + sizeof(return_data_msg_t)
				);
			}
			else
			{
				printf("Trying to forward evaluated predicate to: %s\n",
					addr2str(&data->originator));

				send_evaluated_predicate(conn,
										 &msg->sender, // Source
										 &data->originator, // Destination
										 data->message_id,
										 ((char *)msg) + sizeof(return_data_msg_t)
										);
			}
			break;
		}
	}
}

static void runicast_sent(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	//printf("runicast sent\n");
}

static void runicast_timedout(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("Runicast timed out to:%s retransmissions:%d\n", addr2str(to), retransmissions);
}


// Callbacks
static const struct runicast_callbacks runicastCallbacks =
	{ &runicast_recv, &runicast_sent, &runicast_timedout };

static const struct stbroadcast_callbacks stbroadcastCallbacks =
	{ &stbroadcast_recv, &stbroadcast_sent };


// Methods
static void delayed_send_evaluated_predicate(void * ptr)
{
	printf("Starting delayed send of evaluated predicate\n");

	delayed_send_evaluated_predicate_params_t * p =
		(delayed_send_evaluated_predicate_params_t *)ptr;

	array_list_elem_t elem;
	for (elem = array_list_first(&p->conn->message_list); array_list_continue(&p->conn->message_list, elem); elem = array_list_next(elem))
	{
		list_elem_t * data = (list_elem_t *)array_list_data(&p->conn->message_list, elem);

		if (data->message_id == p->message_id)
		{
			send_evaluated_predicate(p->conn,
									 &rimeaddr_node_addr, // Source
									 &data->originator, // Destination
									 data->message_id,
									 NULL
									);

			// TODO: remove item from the list
			break;
		}
	}

	// Need to free allocated parameter struct
	free(ptr);
}

static void
delayed_forward_evaluated_predicate(void * ptr)
{
	delayed_forward_evaluated_predicate_params_t * p =
		(delayed_forward_evaluated_predicate_params_t *)ptr;

	void * data_dest = ((char *)p) + sizeof(delayed_forward_evaluated_predicate_params_t);

	send_evaluated_predicate(p->conn,
		&p->msg.sender, &p->msg.target_receiver,
		p->msg.message_id, data_dest);

	// Need to free allocated parameter struct
	free(ptr);
}

static void
send_evaluated_predicate(
	nhopreq_conn_t * hc, rimeaddr_t const * sender, rimeaddr_t const * target_receiver,
	uint8_t message_id, void const * data)
{
	if (runicast_is_transmitting(&hc->ru))
	{
		printf("runicast is already transmitting, trying again in a few seconds\n");

		delayed_forward_evaluated_predicate_params_t * p =
			(delayed_forward_evaluated_predicate_params_t *)
				malloc(sizeof(delayed_forward_evaluated_predicate_params_t) + hc->data_size);

		rimeaddr_copy(&p->msg.sender, sender);
		rimeaddr_copy(&p->msg.target_receiver, target_receiver);
		p->msg.message_id = message_id;

		p->conn = hc;

		void * data_dest = ((char *)p) + sizeof(delayed_forward_evaluated_predicate_params_t);

		if (data == NULL)
		{
			// Call data get functions and store result in outwards bound packet
			(*hc->data_fn)(data_dest);
		}
		else
		{
			// Copy the provided data
			memcpy(data_dest, data, hc->data_size);
		}

		static struct ctimer forward_timer;
		ctimer_set(&forward_timer, 5 * CLOCK_SECOND, &delayed_forward_evaluated_predicate, p);
	}
	else
	{
		unsigned int packet_size = sizeof(return_data_msg_t) + hc->data_size;

		packetbuf_clear();
		packetbuf_set_datalen(packet_size);
		debug_packet_size(packet_size);
		return_data_msg_t * msg = (return_data_msg_t *)packetbuf_dataptr();
		memset(msg, 0, packet_size);

		rimeaddr_copy(&msg->sender, sender);
		rimeaddr_copy(&msg->target_receiver, target_receiver);
		msg->message_id = message_id;

		void * data_dest = ((char *)msg) + sizeof(return_data_msg_t);

		if (data == NULL)
		{
			// Call data get functions and store result in outwards bound packet
			(*hc->data_fn)(data_dest);
		}
		else
		{
			// Copy the provided data
			memcpy(data_dest, data, hc->data_size);
		}

		runicast_send(&hc->ru, target_receiver, 10);
	}
}



static void
send_n_hop_predicate_check(
	nhopreq_conn_t * conn, rimeaddr_t const * originator,
	uint8_t message_id_to_send, uint8_t hop_limit)
{
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(req_data_msg_t));
	debug_packet_size(sizeof(req_data_msg_t));
	req_data_msg_t * msg = (req_data_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(req_data_msg_t));

	rimeaddr_copy(&msg->originator, originator);
	msg->message_id = message_id_to_send;
	msg->hop_limit = hop_limit;

	random_init(rimeaddr_node_addr.u8[0] + 2);
	unsigned int random = (random_rand() % 5);
	if (random <= 1) random++;

	printf("Starting sbcast every %d second(s) for %d seconds\n", random, 20);

	stbroadcast_send_stubborn(&conn->bc, random * CLOCK_SECOND);

	static struct ctimer stbroadcast_stop_timer;
	ctimer_set(&stbroadcast_stop_timer, 20 * CLOCK_SECOND, &stbroadcast_callback_cancel, conn);
}


// Library Functions

// Initialise multi-hop predicate checking
bool nhopreq_start(
	nhopreq_conn_t * conn, uint8_t ch1, uint8_t ch2, rimeaddr_t const * baseStationAddr,
	data_generation_fn data_fn, unsigned int data_size, data_receive_fn receive_fn)
{
	if (conn == NULL || baseStationAddr == NULL ||
		data_fn == NULL || ch1 == ch2 || data_size == 0 ||
		receive_fn == NULL)
	{
		return false;
	}

	stbroadcast_open(&conn->bc, ch1, &stbroadcastCallbacks);
	runicast_open(&conn->ru, ch2, &runicastCallbacks);

	rimeaddr_copy(&conn->baseStationAddr, baseStationAddr);

	conn->message_id = 1;

	conn->data_fn = data_fn;
	conn->data_size = data_size;
	conn->receive_fn = receive_fn;

	array_list_init(&conn->message_list, &free);

	return true;
}

// Shutdown multi-hop predicate checking
bool nhopreq_end(nhopreq_conn_t * conn)
{
	if (conn == NULL)
	{
		return false;
	}

	runicast_close(&conn->ru);
	stbroadcast_close(&conn->bc);

	// Free List
	array_list_clear(&conn->message_list);

	return true;
}


bool is_base(nhopreq_conn_t const * conn)
{
	return conn != NULL && rimeaddr_cmp(&rimeaddr_node_addr, &conn->baseStationAddr);
}

void nhopreq_request_info(nhopreq_conn_t * conn, uint8_t hops)
{
	list_elem_t * delivered_msg = (list_elem_t *)malloc(sizeof(list_elem_t));

	delivered_msg->message_id = get_message_id(conn);
	delivered_msg->hops = hops;

	// Set the originator to self
	rimeaddr_copy(&delivered_msg->originator, &rimeaddr_node_addr);

	array_list_append(&conn->message_list, delivered_msg);

	send_n_hop_predicate_check(
		conn, &rimeaddr_node_addr,
		delivered_msg->message_id, delivered_msg->hops);
}

static uint8_t get_message_id(nhopreq_conn_t * conn)
{
	uint8_t returnvalue = conn->message_id++;
	returnvalue *= 100;
	returnvalue += rimeaddr_node_addr.u8[0];
	return returnvalue;
}

