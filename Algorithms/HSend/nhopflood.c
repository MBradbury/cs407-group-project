#include "nhopflood.h"

#include "contiki.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "random-range.h"
#include "sensor-converter.h"
#include "debug-helper.h"

// Message Structs
typedef struct
{
	rimeaddr_t originator;
	message_id_t message_id;
	uint8_t max_hops;
	uint8_t current_hops;
	// After this point the user generated data will be contained
} nhopflood_msg_t;

// Argument Params Structs
typedef struct
{
	nhopflood_conn_t * conn;
} nhopflood_timer_params_t;

static inline nhopflood_conn_t * conncvt_flood_message_recv(struct stbroadcast_conn * conn)
{
	return (nhopflood_conn_t *)conn;
}

// Stubborn Broadcast Message Recieved Callback
static void flood_message_recv(struct stbroadcast_conn * c)
{
	// Get a pointer to the nhopflood_conn_t
	nhopflood_conn_t * conn = conncvt_flood_message_recv(c);

	// Copy the message from the packet buffer
	char tmpBuffer[PACKETBUF_SIZE];
	memcpy(tmpBuffer, packetbuf_dataptr(), packetbuf_datalen());
	nhopflood_msg_t const * msg = (nhopflood_msg_t *)tmpBuffer;

	//Increment current_hops
	msg->current_hops++;

	// Print a debug message
	printf("I just recieved a Flood Message! Originator: %s Max Hops: %d Current Hops %d Message ID: %d\n",
		addr2str(&msg->originator),
		msg->max_hops,
		msg->current_hops,
		msg->message_id);

	//TODO: If current_hops <= max_hops, then deliver
	//TODO: If current_hops < max_hops, then enqueue in packet buffer to be retransmitted
}

// Stubborn Broadcast Message Sent Callback 
static void flood_message_sent(struct stbroadcast_conn *c)
{
	//printf("I've sent a nhopflood message!\n");
}

// Setup the Stubborn Broadcast Callbacks
static const struct broadcast_callbacks broadcastCallbacks =
	{ &flood_message_recv, &flood_message_sent };

// Initialise n-hop data flooding.
bool nhopflood_start(
	nhopflood_conn_t * conn, uint8_t ch1, data_generation_fn data_fn, 
	data_receive_fn receive_fn, unsigned int data_size, clock_time_t period, 
	clock_time_t max, clock_time_t send_delay)
{
	if (conn == NULL || data_fn == NULL ||  data_size == 0 ||
		receive_fn == NULL || ch1 == NULL)
	{
		printf("nhopflood_start failed!\n");
		return false;
	}

	// Initialize the packetqueue
	PACKETQUEUE(nhopflood_packetqueue, 100);

	ctimer_init();

	broadcast_open(&conn->bc, ch1, &stbroadcastCallbacks);

	conn->data_fn = data_fn;
	conn->receive_fn = receive_fn;

	conn->data_size = data_size;
	conn->max_hops = 0;
	conn->message_id = 0;

	conn->period = period;
	conn->max = max;
	conn->send_delay = send_delay;

	return true;
}

// Shutdown n-hop data flooding.
bool nhopflood_end(nhopflood_conn_t * conn)
{
	if (conn == NULL)
	{
		printf("nhopflood_end failed!\n");
		return false;
	}

	broadcast_close(&conn->bc);

	return true;
}

// Register a request to send this nodes data n hops next cycle
bool nhopflood_register(nhopflood_conn_t * conn, uint8_t hops)
{
	if (hops == 0)
	{
		printf("Hops cannot be set to 0!\n");
		return false;
	}

	if (conn == NULL)
	{
		printf("The nhopflood_conn is null!\n");
	}

	// If max_hops == 0 then we havent recieved a register request before in this cycle so we should initialise the timer for the delayed start
	if (conn->max_hops == 0)
	{
		nhopflood_timer_params_t * params = (nhopflood_timer_params_t *)malloc(sizeof(nhopflood_timer_params_t));
		params->conn = conn;

		ctimer_set(&conn->delay_timer, conn->send_delay, &nhopflood_delayed_start_sending, conn);
	}

	// If this request is higher than 
	if (conn->max_hops < hops)
	{
		conn->max_hops = hops;
	}

	return true;
}

void nhopflood_delayed_start_sending(void * ptr)
{
	// Get the conn from the pointer provided
	nhopflood_conn_t * conn = (nhopflood_conn_t *)ptr;

	// Calculate the size of the packet
	unsigned int packet_size = sizeof(nhopflood_msg_t) + conn->data_size;

	// Create the memory for the packet to broadcast
	packetbuf_clear();
	packetbuf_set_datalen(packet_size);
	debug_packet_size(packet_size);
	nhopflood_msg_t * msg = (nhopflood_msg_t *)packetbuf_dataptr();
	memset(msg, 0, packet_size);

	// Populate the packet with our data
	rimeaddr_copy(&msg->originator, &rimeaddr_node_addr);
	msg->message_id = conn->message_id;
	msg->max_hops = conn->max_hops;
	msg->current_hops = 0;

	// Get the data to send from the data function
	void * msg_data = (void *)(msg + 1);
	(*conn->conn->data_fn)(msg_data);

	// Initialize the packet queue
	packetqueue_init(&nhopflood_packetqueue);
	
	// Enqueue the packet previously created
	packetqueue_enqueue_packetbuf(&nhopflood_packetqueue, 0, );

	// Start a timer to send the first packet in the queue every interval
	ctimer_set(&conn->send_timer, &conn->period, &nhopflood_delayed_repeat_send, conn);

	// Start a timer to cancel the broadcast after max time has been reached
	ctimer_set(&conn->cancel_timer, &conn->max, &nhopflood_delayed_cancel, conn);
}

void nhopflood_delayed_repeat_send(void * ptr)
{
	// Get the conn from the pointer provided
	nhopflood_conn_t * conn = (nhopflood_conn_t *)ptr;

	//TODO: Get the first packet from the packet queue
	//TODO: Dequeue the packet from the packet queue
	//TODO: If its from this node and isn't out of date copy it and re-enqueue it
	
	//TODO: Send the packet using normal broadcast
	broadcast_send(&conn);

	// Reset the timer to call this function again
	ctimer_reset(&conn->send_timer);
}

void nhopflood_delayed_cancel(void * ptr)
{
	//TODO: Cancel the nhopflood_delayed_repeat_send_timer
	//TODO: Clear the packet queue

	// Get the params from the pointer provided
	nhopflood_timer_params_t * params = (nhopflood_timer_params_t *)ptr;

	// Cancel the stubborn broadcast
	stbroadcast_cancel(params->conn->bc);

	// Reset the max_hops to start the cycle again
	params->conn->max_hops = 0;
	params->conn->message_id++;

	// Finally we free the allocated params structure.
	free(ptr);
}

void nhopflood_broadcast_packet()
{

}