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
    char const * predicate_to_check;
} list_elem_struct_t;

// Struct used to ask other nodes for predicate values
typedef struct
{
    rimeaddr_t originator;
    uint8_t message_id;
    uint8_t hop_limit;
    char const * predicate_to_check;
} predicate_check_msg_t;

// Struct to send back to the originator with the value of a predicate
typedef struct
{
    rimeaddr_t sender;
    rimeaddr_t target_reciever;
    uint8_t message_id;
    char const * evaluated_predicate;
} predicate_return_msg_t;


static hsend_conn_t * conncvt_runicast(struct runicast_conn * conn)
{
	return (hsend_conn_t *)conn;
}

static hsend_conn_t * conncvt_stbcast(struct stbroadcast_conn * conn)
{
	return (hsend_conn_t *)
		(((char *)conn) - sizeof(struct stbroadcast_conn));
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
	predicate_return_msg_t msg;
} delayed_forward_evaluated_predicate_params_t;


// Prototypes
static void
delayed_send_evaluated_predicate(void * ptr);

static void
send_n_hop_predicate_check(
	hsend_conn_t * conn, rimeaddr_t const *originator, uint8_t message_id_to_send,
	char const *pred, uint8_t hop_limit);

static void
send_evaluated_predicate(
	hsend_conn_t * conn, rimeaddr_t const * sender, rimeaddr_t const * target_reciever,
	uint8_t message_id, char const * evaluated_predicate);

static uint8_t
get_message_id(hsend_conn_t * conn);

static char const *
evaluate_predicate(char const * predicate);

// STUBBORN BROADCAST
static void
stbroadcast_recv(struct stbroadcast_conn *c)
{
	hsend_conn_t * hc = conncvt_stbcast(c);

    // Copy Packet Buffer To Memory
    char tmpBuffer[PACKETBUF_SIZE];
    packetbuf_copyto(tmpBuffer);
    predicate_check_msg_t *msg = (predicate_check_msg_t *)tmpBuffer;

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
        delivered_msg->predicate_to_check = msg->predicate_to_check;

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
            send_n_hop_predicate_check(hc, &rimeaddr_node_addr, msg->message_id,
				msg->predicate_to_check, msg->hop_limit - 1);
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

    printf("runicast received from %s\n", addr2str(from));

    // When recieve message, forward the message on to the originator
    // if the final originator, do something with the value

    // Copy Packet Buffer To Memory
    char tmpBuffer[PACKETBUF_SIZE];
    packetbuf_copyto(tmpBuffer);
    predicate_return_msg_t * msg = (predicate_return_msg_t *)tmpBuffer;


    list_elem_struct_t *list_iterator = NULL;
    for (list_iterator = (list_elem_struct_t *)list_head(conn->message_list);
         list_iterator != NULL;
         list_iterator = (list_elem_struct_t *)list_item_next(list_iterator))
    {
        if (list_iterator->message_id == msg->message_id)
        {
			// If this node was the one who sent the message
            if (rimeaddr_cmp(&rimeaddr_node_addr, &list_iterator->originator)) 
            {
                printf("End of forwarding Value: %s received from: %s\n",
					msg->evaluated_predicate, addr2str(&msg->sender));
            }
            else
            {
                rimeaddr_t dest;
                rimeaddr_copy(&dest, &list_iterator->originator);

                printf("Trying to forward evaluated predicate to: %s\n",
					addr2str(&dest));

                send_evaluated_predicate(conn,
										 &msg->sender,
                                         &dest,
                                         list_iterator->message_id,
                                         msg->evaluated_predicate
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
runicast_sent(struct runicast_conn *c, rimeaddr_t const * to, uint8_t retransmissions)
{
    //printf("runicast sent\n");
}

static void
runicast_timedout(struct runicast_conn *c, rimeaddr_t const * to, uint8_t retransmissions)
{
    printf("Runicast timed out to:%s retransmissions:%d\n", addr2str(to), retransmissions);
}


//Callbacks
static const struct runicast_callbacks runicastCallbacks = { runicast_recv, runicast_sent, runicast_timedout };
static const struct stbroadcast_callbacks stbroadcastCallbacks = { stbroadcast_recv, stbroadcast_sent };


//METHODS
static void
delayed_send_evaluated_predicate(void * ptr)
{
    printf("Starting delayed send of evaluated predicate\n");
	delayed_send_evaluated_predicate_params_t * p = (delayed_send_evaluated_predicate_params_t *)ptr;

    list_elem_struct_t * list_iterator = NULL;
    for (list_iterator = (list_elem_struct_t *)list_head(p->conn->message_list);
         list_iterator != NULL;
         list_iterator = (list_elem_struct_t *)list_item_next(list_iterator))
    {
        if (list_iterator->message_id == p->message_id)
        {
            rimeaddr_t dest;
            rimeaddr_copy(&dest, &list_iterator->originator);

            send_evaluated_predicate(p->conn,
								     &rimeaddr_node_addr,
                                     &dest,
                                     list_iterator->message_id,
                                     evaluate_predicate(list_iterator->predicate_to_check)
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

static char const *
evaluate_predicate(char const *predicate)
{
    return "Value";
}

static void
delayed_forward_evaluated_predicate(void * ptr)
{
    delayed_forward_evaluated_predicate_params_t * p = (delayed_forward_evaluated_predicate_params_t *)ptr;

    send_evaluated_predicate(p->conn,
		&p->msg.sender, &p->msg.target_reciever,
		p->msg.message_id, p->msg.evaluated_predicate);

    free(ptr);
}

static void
send_evaluated_predicate(
	hsend_conn_t * hc, rimeaddr_t const * sender, rimeaddr_t const * target_reciever,
	uint8_t message_id, char const * evaluated_predicate)
{

    if (runicast_is_transmitting(&hc->ru))
    {
        printf("runicast is already transmitting, trying again in a few seconds\n");
        static struct ctimer forward_timer;

        delayed_forward_evaluated_predicate_params_t * p =
			(delayed_forward_evaluated_predicate_params_t *)
				malloc(sizeof(delayed_forward_evaluated_predicate_params_t));

        rimeaddr_copy(&p->msg.sender, sender);
        rimeaddr_copy(&p->msg.target_reciever, target_reciever);
        p->msg.message_id = message_id;
        p->msg.evaluated_predicate = evaluated_predicate;

		p->conn = hc;

        ctimer_set(&forward_timer, 5 * CLOCK_SECOND, &delayed_forward_evaluated_predicate, p);

        return;
    }

    packetbuf_clear();
    packetbuf_set_datalen(sizeof(predicate_return_msg_t));
    debug_packet_size(sizeof(predicate_return_msg_t));
    predicate_return_msg_t *msg = (predicate_return_msg_t *)packetbuf_dataptr();
    memset(msg, 0, sizeof(predicate_return_msg_t));

    rimeaddr_copy(&msg->sender, sender);
    rimeaddr_copy(&msg->target_reciever, target_reciever);
    msg->message_id = message_id;
    msg->evaluated_predicate = evaluated_predicate;

    runicast_send(&hc->ru, target_reciever, 10);
}



static void
send_n_hop_predicate_check(
	hsend_conn_t * conn, rimeaddr_t const * originator,
	uint8_t message_id_to_send, char const * pred, uint8_t hop_limit)
{
    packetbuf_clear();
    packetbuf_set_datalen(sizeof(predicate_check_msg_t));
    debug_packet_size(sizeof(predicate_check_msg_t));
    predicate_check_msg_t * msg = (predicate_check_msg_t *)packetbuf_dataptr();
    memset(msg, 0, sizeof(predicate_check_msg_t));

    rimeaddr_copy(&msg->originator, originator);
    msg->message_id = message_id_to_send;
    msg->predicate_to_check = pred;
    msg->hop_limit = hop_limit;

    random_init(rimeaddr_node_addr.u8[0] + 2);
    int random = (random_rand() % 5);
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
	data_generation_fn data_fn, unsigned int data_size)
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
		rimeaddr_cpy(&nd->addr, &rimeaddr_node_addr);

		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		nd->temp = sht11_temperature(raw_temperature);
		nd->humidity = sht11_relative_humidity_compensated(raw_humidity, nd->temp);
	}
}


static hsend_conn_t hc;

PROCESS(mainProcess, "HSEND Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
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

	if (!hsend_start(&hc, 149, 132, &baseStationAddr, &node_data, sizeof(node_data_t)))
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

                struct list_elem_struct * delivered_msg =  (struct list_elem_struct *)malloc(sizeof(struct list_elem_struct));
                delivered_msg->message_id = get_message_id(&hc);
                delivered_msg->hops = 3;
                // set the originator to self
                rimeaddr_copy(&delivered_msg->originator, &rimeaddr_node_addr);

                list_push(hc.message_list, delivered_msg);

                send_n_hop_predicate_check(&hc, &rimeaddr_node_addr, delivered_msg->message_id, "Hello World!!!", delivered_msg->hops);
            }

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }
    }

exit:
    printf("Exiting Process...\n");
    hsend_end(&hc);
    PROCESS_END();
}

