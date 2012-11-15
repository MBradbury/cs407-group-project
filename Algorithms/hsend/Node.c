#include "contiki.h"

#include "dev/leds.h"
#include "lib/list.h"

#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/stbroadcast.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lib/random.h"
#include "debug-helper.h"

static struct mesh_conn mesh;
static struct stbroadcast_conn stbroadcast;

static rimeaddr_t baseStationAddr;

static unsigned int message_id = 1;

//Methods
static void 
send_n_hop_predicate_check(rimeaddr_t const * originator, unsigned int message_id, char const * pred, unsigned int hop_limit);
static void
send_predicate_to_node(rimeaddr_t const * dest, char const * pred);

struct list_elem_struct
{
	struct list_elem_struct *next;
	unsigned int message_id;
	unsigned int hops;
};

LIST(message_list);

typedef struct
{
	rimeaddr_t originator;
	unsigned int message_id;
	unsigned int hop_limit;
	char * predicate_to_check;
} predicate_check_msg_t;

static bool 
is_base(void)
{
	static rimeaddr_t base;
	memset(&base, 0, sizeof(rimeaddr_t));
	base.u8[sizeof(rimeaddr_t) - 2] = 1;

	return rimeaddr_cmp(&rimeaddr_node_addr, &base) != 0;
}

static uint8_t 
get_message_id(void)
{
	uint8_t returnvalue;

	returnvalue = message_id++;

	returnvalue *= 100; 

	returnvalue += rimeaddr_node_addr.u8[0];

	printf("%d\n", returnvalue);

	return returnvalue;
}

/** The function that will be executed when a message is received */
static void 
mesh_recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
	if(is_base())
	{
		printf("Mesh Message received from: %s message: %s\n",
			addr2str(from),
			(char const *)packetbuf_dataptr());
	}
	else
	{
		printf("Mesh Predicate received: %s from: %s\n",
			(char const *)packetbuf_dataptr(),
			addr2str(from));
	}
}	

static void
mesh_sent(struct mesh_conn *c)
{
	if(is_base())
	{

	}
	else
	{
  	//	printf("mesh packet sent\n");
	}
}

static void
mesh_timedout(struct mesh_conn *c)
{
  //	printf("mesh packet timedout\n");

	if(is_base())
	{

	}
	else
	{

	}
}

static void
stbroadcast_recv(struct stbroadcast_conn *c)
{
	predicate_check_msg_t const * msg = (predicate_check_msg_t const *)packetbuf_dataptr();
	
	printf("I just recieved a Stubborn Broadcast Message!\n");
	printf("%d\n", msg->hop_limit);
	printf("%s\n", msg->predicate_to_check);

	// Check message has not been recieved before
	bool deliver_msg = false;

	struct list_elem_struct * list_iterator = NULL;
	for ( list_iterator = (struct list_elem_struct *)list_head(message_list);
		  list_iterator != NULL;
		  list_iterator = (struct list_elem_struct *)list_item_next(&list_iterator)
		)
	{
		// Message has been delivered before
		if (list_iterator->message_id == msg->message_id)
		{
			// If the new message has a higher hop count
			if (msg->hop_limit > list_iterator->hops)
			{
				printf("Message received before and hops is higher\n");

				list_iterator->hops = msg->hop_limit;
				deliver_msg = true;
			}

			break;
		} 
	}
	
	// End of List and the Message has NOT been delivered before
	if (list_iterator == NULL)
	{
		struct list_elem_struct * delivered_msg = (struct list_elem_struct *)malloc(sizeof(struct list_elem_struct));
		delivered_msg->message_id = msg->message_id;
		delivered_msg->hops = msg->hop_limit;

		list_push(message_list, delivered_msg);

		deliver_msg = true;
	}

	if (deliver_msg) 
	{
		// Send predicate value back to originator		
		send_predicate_to_node(&msg->originator, "Value");

		if (msg->hop_limit > 1) //last node 
		{
			static char addr_str[RIMEADDR_STRING_LENGTH];
			static char addr_str2[RIMEADDR_STRING_LENGTH];

			printf("Node %s is resending message from: %s hop_limit:%d\n",
				addr2str_r(&rimeaddr_node_addr, addr_str, RIMEADDR_STRING_LENGTH),
				addr2str_r(&msg->originator, addr_str2, RIMEADDR_STRING_LENGTH),
				msg->hop_limit);

			// Send message on with one less hop limit
			send_n_hop_predicate_check(&msg->originator, msg->message_id, msg->predicate_to_check, msg->hop_limit - 1);
		}
	}
}

static void
stbroadcast_sent(struct stbroadcast_conn *c)
{
	//printf("I've sent!\n");
}

static const struct mesh_callbacks meshCallbacks = {mesh_recv, mesh_sent, mesh_timedout};
static const struct stbroadcast_callbacks stbroadcastCallbacks = {stbroadcast_recv, stbroadcast_sent};

static void
cancel_stbroadcast(void * ptr)
{
	printf("Canceling\n");
	stbroadcast_cancel(&stbroadcast);
}

static void
send_predicate_to_node(rimeaddr_t const * dest, char const * pred)
{
	packetbuf_clear();
	packetbuf_set_datalen(strlen(pred));
	debug_packet_size(strlen(pred));
	packetbuf_copyfrom(pred, strlen(pred));

	mesh_send(&mesh, dest);

}

static void 
send_to_base_station(char const * message)
{
	memset(&baseStationAddr, 0, sizeof(rimeaddr_t));
	baseStationAddr.u8[sizeof(rimeaddr_t) - 2] = 1;

	packetbuf_clear();
	packetbuf_set_datalen(strlen(message));
	debug_packet_size(strlen(message));
	packetbuf_copyfrom(message, strlen(message));

	mesh_send(&mesh, &baseStationAddr);
}

static void
send_n_hop_predicate_check(rimeaddr_t const * originator, unsigned int message_id_to_send, char const * pred, unsigned int hop_limit)
{
	printf("%d\n", hop_limit);

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(predicate_check_msg_t));
	debug_packet_size(sizeof(predicate_check_msg_t));
	predicate_check_msg_t * msg = (predicate_check_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(predicate_check_msg_t));
	
	rimeaddr_copy(&msg->originator, originator);
	msg->message_id = message_id_to_send;
	msg->predicate_to_check = pred;
	msg->hop_limit = hop_limit;

	printf("%d\n", msg->hop_limit);

	random_init(rimeaddr_node_addr.u8[0]+2);
	int random = (random_rand() % 5);
	if (random <= 1) random++;

	stbroadcast_send_stubborn(&stbroadcast, random*CLOCK_SECOND);	
	
	static struct ctimer stbroadcast_stop_timer;
	ctimer_set(&stbroadcast_stop_timer, 20 * CLOCK_SECOND, &cancel_stbroadcast, NULL);
}

PROCESS(networkInit, "Network Init");
PROCESS(mainProcess, "Main Predicate Checker Process");

AUTOSTART_PROCESSES(&networkInit, &mainProcess);

PROCESS_THREAD(networkInit, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	mesh_open(&mesh, 147, &meshCallbacks);

	// Set the base station
	memset(&baseStationAddr, 0, sizeof(rimeaddr_t));
	baseStationAddr.u8[sizeof(rimeaddr_t) - 2] = 1;


	//5 second timer
	etimer_set(&et, 5 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	PROCESS_END();
}

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	if (is_base()) //SINK
	{
		leds_on(LEDS_BLUE);

		while (true)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
	else //NODE
	{
		stbroadcast_open(&stbroadcast, 132, &stbroadcastCallbacks);

		list_init(message_list);

		etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		
		while (true)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			static rimeaddr_t test;
			memset(&test, 0, sizeof(rimeaddr_t));
			test.u8[sizeof(rimeaddr_t) - 2] = 2;

			static int count = 0;

			if (rimeaddr_cmp(&rimeaddr_node_addr, &test) && count++ == 0)
			{
				struct list_elem_struct * delivered_msg =  (struct list_elem_struct *)malloc(sizeof(struct list_elem_struct));
				delivered_msg->message_id = get_message_id();
				delivered_msg->hops = 2;

				printf("%d\n", delivered_msg->hops); //2
				list_push(message_list, delivered_msg);
				printf("%d\n", delivered_msg->hops); //0

				send_n_hop_predicate_check(&rimeaddr_node_addr, delivered_msg->message_id, "Hello World!!!", delivered_msg->hops);
			}

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}

	exit:
		printf("Exiting Process...\n");
		mesh_close(&mesh);
		stbroadcast_close(&stbroadcast);
		PROCESS_END();
}
