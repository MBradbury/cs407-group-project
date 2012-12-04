#include "hsend.h"

#include "contiki.h"

#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lib/random.h"

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "sensor-converter.h"
#include "debug-helper.h"

// Struct for the list elements, used to see if messages have already been sent
typedef struct list_elem_struct
{
    struct list_elem_struct * next;
    rimeaddr_t originator;
    uint8_t message_id;
    uint8_t hops;
} list_elem_struct_t;

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


static hsend_conn_t * conncvt_runicast(struct runicast_conn * conn)
{
	return (hsend_conn_t *)conn;
}

static hsend_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (hsend_conn_t *)
		(((char *)conn) - sizeof(struct runicast_conn));
}


// Argument structs
typedef struct
{
	hsend_conn_t * conn;
	uint8_t message_id;
} delayed_send_evaluated_predicate_params_t;

typedef struct
{
	hsend_conn_t * conn;
	return_data_msg_t msg;
} delayed_forward_evaluated_predicate_params_t;


// Prototypes
static void
delayed_send_evaluated_predicate(void * ptr);

static void
send_n_hop_predicate_check(
	hsend_conn_t * conn, rimeaddr_t const *originator,
	uint8_t message_id_to_send, uint8_t hop_limit);

static void
send_evaluated_predicate(
	hsend_conn_t * conn, rimeaddr_t const * sender,
	rimeaddr_t const * target_receiver, uint8_t message_id);

static uint8_t
get_message_id(hsend_conn_t * conn);

// STUBBORN BROADCAST
static void
stbroadcast_recv(struct stbroadcast_conn * c)
{
	hsend_conn_t * hc = conncvt_stbcast(c);

	// Copy Packet Buffer To Memory
	char tmpBuffer[PACKETBUF_SIZE];
	packetbuf_copyto(tmpBuffer);
	req_data_msg_t * msg = (req_data_msg_t *)tmpBuffer;

    //  printf("I just recieved a Stubborn Broadcast Message! Originator: %s Message: %s Hop: %d Message ID: %d\n",
    //      addr2str(&msg->originator),
    //      msg->predicate_to_check,
    //      msg->hop_limit,
    //      msg->message_id);

    // Check message has not been received before
    bool deliver_msg = false;
    list_elem_struct_t * list_iterator = NULL;
    for (list_iterator = (list_elem_struct_t *)list_head(hc->message_list);
         list_iterator != NULL;
         list_iterator = (list_elem_struct_t *)list_item_next(list_iterator))
    {
        // Message has been delivered before
        if (list_iterator->message_id == msg->message_id)
        {
            // If the new message has a higher hop count
            if (msg->hop_limit > list_iterator->hops)
            {
                printf("Message received before and hops is higher\n");

                // Update the new originator and hop count
                rimeaddr_copy(&list_iterator->originator, &msg->originator);

                list_iterator->hops = msg->hop_limit;

                deliver_msg = true;
            }
            break;
        }
    }

    // End of List and the Message has NOT been delivered before
    if (list_iterator == NULL)
    {
        list_elem_struct_t * delivered_msg =
			(list_elem_struct_t *)malloc(sizeof(list_elem_struct_t));

        rimeaddr_copy(&delivered_msg->originator, &msg->originator);
        delivered_msg->message_id = msg->message_id;
        delivered_msg->hops = msg->hop_limit;

        list_push(hc->message_list, delivered_msg);

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

static void
stbroadcast_sent(struct stbroadcast_conn *c)
{
    //printf("I've sent!\n");
}

static void
stbroadcast_callback_cancel(void *ptr)
{
	hsend_conn_t * conn = (hsend_conn_t *)ptr;

    printf("Canceling Stubborn Broadcast.\n");
    stbroadcast_cancel(&conn->bc);
}

// TODO: need to collate responses, then send the on,
// right now messages collide while a node is trying to forward
// RELIABLE UNICAST
static void
runicast_recv(struct runicast_conn * c, rimeaddr_t const * from, uint8_t seqno)
{
	hsend_conn_t * conn = conncvt_runicast(c);

    //printf("runicast received from %s\n", addr2str(from));

    // When recieve message, forward the message on to the originator
    // if the final originator, do something with the value

    // Copy Packet Buffer To Memory
    char tmpBuffer[PACKETBUF_SIZE];
    packetbuf_copyto(tmpBuffer);
    return_data_msg_t * msg = (return_data_msg_t *)tmpBuffer;


    list_elem_struct_t * list_iterator = NULL;
    for (list_iterator = (list_elem_struct_t *)list_head(conn->message_list);
         list_iterator != NULL;
         list_iterator = (list_elem_struct_t *)list_item_next(list_iterator))
    {
        if (list_iterator->message_id == msg->message_id)
        {
			// If this node was the one who sent the message
            if (rimeaddr_cmp(&rimeaddr_node_addr, &list_iterator->originator)) 
            {
				// The target node has received the required data,
				// so provide it to the upper layer
				(*conn->receive_fn)(&msg->sender, ((char *)msg) + sizeof(return_data_msg_t));
            }
            else
            {
                printf("Trying to forward evaluated predicate to: %s\n",
					addr2str(&list_iterator->originator));

                send_evaluated_predicate(conn,
										 &msg->sender, // Source
                                         &list_iterator->originator, // Destination
                                         list_iterator->message_id
                                        );
            }
            break;
        }
    }

    if (list_iterator == NULL)
    {
        printf("DEBUG: ERROR - LIST IS NULL, THIS IS VERY VERY BAD\n");
    }

}

static void
runicast_sent(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
    //printf("runicast sent\n");
}

static void
runicast_timedout(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
    printf("Runicast timed out to:%s retransmissions:%d\n", addr2str(to), retransmissions);
}


//Callbacks
static const struct runicast_callbacks runicastCallbacks =
	{ runicast_recv, runicast_sent, runicast_timedout };

static const struct stbroadcast_callbacks stbroadcastCallbacks =
	{ stbroadcast_recv, stbroadcast_sent };


//METHODS
static void
delayed_send_evaluated_predicate(void * ptr)
{
    printf("Starting delayed send of evaluated predicate\n");
	delayed_send_evaluated_predicate_params_t * p =
		(delayed_send_evaluated_predicate_params_t *)ptr;

    list_elem_struct_t * list_iterator = NULL;
    for (list_iterator = (list_elem_struct_t *)list_head(p->conn->message_list);
         list_iterator != NULL;
         list_iterator = (list_elem_struct_t *)list_item_next(list_iterator))
    {
        if (list_iterator->message_id == p->message_id)
        {
            send_evaluated_predicate(p->conn,
								     &rimeaddr_node_addr, // Source
                                     &list_iterator->originator, // Destination
                                     list_iterator->message_id
                                    );

            // TODO: remove item from the list
            break;
        }
    }

    if (list_iterator == NULL)
    {
        printf("DEBUG: ERROR - LIST IS NULL, THIS IS VERY BAD\n");
    }

	// Need to free allocated parameter struct
	free(ptr);
}

static void
delayed_forward_evaluated_predicate(void * ptr)
{
    delayed_forward_evaluated_predicate_params_t * p =
		(delayed_forward_evaluated_predicate_params_t *)ptr;

    send_evaluated_predicate(p->conn,
		&p->msg.sender, &p->msg.target_receiver,
		p->msg.message_id);

    // Need to free allocated parameter struct
	free(ptr);
}

static void
send_evaluated_predicate(
	hsend_conn_t * hc, rimeaddr_t const * sender,
	rimeaddr_t const * target_receiver, uint8_t message_id)
{
    if (runicast_is_transmitting(&hc->ru))
    {
        printf("runicast is already transmitting, trying again in a few seconds\n");

        delayed_forward_evaluated_predicate_params_t * p =
			(delayed_forward_evaluated_predicate_params_t *)
				malloc(sizeof(delayed_forward_evaluated_predicate_params_t));

        rimeaddr_copy(&p->msg.sender, sender);
        rimeaddr_copy(&p->msg.target_receiver, target_receiver);
        p->msg.message_id = message_id;

		p->conn = hc;

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

		// Call data get functions and store result in outwards bound packet
		(*hc->data_fn)(((char *)msg) + sizeof(return_data_msg_t));

		runicast_send(&hc->ru, target_receiver, 10);
	}
}



static void
send_n_hop_predicate_check(
	hsend_conn_t * conn, rimeaddr_t const * originator,
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
bool hsend_start(
	hsend_conn_t * conn, uint8_t ch1, uint8_t ch2, rimeaddr_t const * baseStationAddr,
	data_generation_fn data_fn, unsigned int data_size, data_receive_fn receive_fn)
{
	if (conn == NULL || baseStationAddr == NULL ||
		data_fn == NULL || ch1 == ch2 || data_size == 0)
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

	LIST_STRUCT_INIT(conn, message_list);

	return true;
}

// Shutdown multi-hop predicate checking
bool hsend_end(hsend_conn_t * conn)
{
	if (conn == NULL)
	{
		return false;
	}

	runicast_close(&conn->ru);
    stbroadcast_close(&conn->bc);

	// TODO: Free List

	return true;
}


bool
is_base(hsend_conn_t const * conn)
{
    return rimeaddr_cmp(&rimeaddr_node_addr, &conn->baseStationAddr) != 0;
}

static uint8_t
get_message_id(hsend_conn_t * conn)
{
    uint8_t returnvalue = conn->message_id++;
    returnvalue *= 100;
    returnvalue += rimeaddr_node_addr.u8[0];
    return returnvalue;
}


//////////////////////////////
/// NORMAL PROCESS STARTS HERE
//////////////////////////////

typedef struct
{
	rimeaddr_t addr;
	double temp;
	double humidity;
} node_data_t;

static void node_data(void * data)
{
	if (data != NULL)
	{
		node_data_t * nd = (node_data_t *)data;

		// Store the current nodes address
		rimeaddr_copy(&nd->addr, &rimeaddr_node_addr);

		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		nd->temp = sht11_temperature(raw_temperature);
		nd->humidity = sht11_relative_humidity_compensated(raw_humidity, nd->temp);
	}
}

static void receieved_data(rimeaddr_t const * from, void const * data)
{
	node_data_t const * nd = (node_data_t const *)data;

	printf("Obtained information from %s T:%d H:%d%%\n", addr2str(from), (int)nd->temp, (int)nd->humidity);
}


PROCESS(mainProcess, "HSEND Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static hsend_conn_t hc;
	static rimeaddr_t baseStationAddr, test;
    static struct etimer et;

    PROCESS_EXITHANDLER(goto exit;)
    PROCESS_BEGIN();

	// Set the address of the base station
	baseStationAddr.u8[0] = 1;
	baseStationAddr.u8[1] = 0;

	// Set the id of the node that will do the testing
	test.u8[0] = 2;
	test.u8[1] = 0;

	if (!hsend_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t), &receieved_data))
	{
		printf("start function failed\n");
	}

	// 10 second timer
	etimer_set(&et, 10 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	printf("Starting loops:\n");

    if (is_base(&hc)) //SINK
    {
		printf("Is the base station!\n");

        leds_on(LEDS_BLUE);

        while (true)
        {
			etimer_set(&et, 10 * CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }
    }
    else //NODE
    {
        static int count = 0;

        while (true)
        {
			etimer_reset(&et);

            if (rimeaddr_cmp(&rimeaddr_node_addr, &test) && count++ == 0)
            {
				printf("Sending pred req\n");

                list_elem_struct_t * delivered_msg =
					(list_elem_struct_t *)malloc(sizeof(list_elem_struct_t));

                delivered_msg->message_id = get_message_id(&hc);
                delivered_msg->hops = 3;
                // set the originator to self
                rimeaddr_copy(&delivered_msg->originator, &rimeaddr_node_addr);

                list_push(hc.message_list, delivered_msg);

                send_n_hop_predicate_check(
					&hc, &rimeaddr_node_addr,
					delivered_msg->message_id, delivered_msg->hops);
            }

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }
    }

exit:
    printf("Exiting Process...\n");
    hsend_end(&hc);
    PROCESS_END();
}

