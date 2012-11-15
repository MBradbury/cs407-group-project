#include "contiki.h"

#include "dev/leds.h"
#include "lib/list.h"

#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/stbroadcast.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "lib/random.h"
#include "debug-helper.h"

static struct mesh_conn mesh;
static struct stbroadcast_conn stbroadcast;

static rimeaddr_t baseStationAddr;

static uint8_t message_id = 10;
static uint8_t message_id_received;

//static uint8_t highest_hops_seen = 0;

static list_t message_list;

//Methods
static void 
send_n_hop_predicate_check(rimeaddr_t originator, uint8_t message_id, char* pred, uint8_t hop_limit);
static void
send_predicate_to_node(rimeaddr_t dest, char * pred);
typedef struct 
{
	uint8_t message_id;
	uint8_t hops;
} list_elem_t;

typedef struct
{
	rimeaddr_t originator;
	uint8_t message_id;
	char* predicate_to_check;
	uint8_t hop_limit;
} predicate_check_msg_t;

static bool 
is_base()
{
	static rimeaddr_t base;
	memset(&base, 0, sizeof(rimeaddr_t));
	base.u8[sizeof(rimeaddr_t) - 2] = 1;

	return rimeaddr_cmp(&rimeaddr_node_addr, &base) != 0;
}

static uint8_t 
get_message_id()
{
	return message_id++;
}

/** The function that will be executed when a message is received */
static void 
mesh_recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
	if(is_base())
	{
		printf("Mesh Message received from: %s message: %s\n",addr2str(from),(char *)packetbuf_dataptr());
	}
	else
	{
		printf("Mesh Predicate received: %s from:%s \n",(char *)packetbuf_dataptr(),addr2str(from) );
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
	
	//check message has not been recieved before
	bool deliver_msg = false;
	
	list_elem_t * list_iterator = (list_elem_t *)list_head(&message_list);

	list_elem_t * delivered_msg;
		delivered_msg->message_id = msg->message_id;
		delivered_msg->hops = msg->hop_limit;
	do
	{
		if (list_iterator == NULL) //End of List and the Message has NOT been delivered before
		{
			deliver_msg = true;

			list_push(&message_list, &delivered_msg);

			break;
		}
		else if(list_iterator->message_id == delivered_msg->message_id) //Message has been delivered before
		{
			if(delivered_msg->hops > list_iterator->hops) //if the new message has a higher hop cout
			{
				list_iterator->hops = delivered_msg->hops;
				deliver_msg = true;
			}
			break;
		} 
		else //Haven't found message yet and not at the end of the list
		{
			list_iterator = (list_elem_t *)list_item_next(&message_list);
		}
	}
	while(true);

	if (deliver_msg) 
	{

		if (msg->hop_limit > 1) //last node 
		{
			printf("Node %s is resending\n",addr2str(&rimeaddr_node_addr) );
			//send message on with one less hop limit
			send_n_hop_predicate_check(msg->originator,msg->message_id, msg->predicate_to_check, msg->hop_limit - 1);
		}
		//send predicate value back to originator		
		send_predicate_to_node(msg->originator,"Value");
	}
}

static void
stbroadcast_sent(struct stbroadcast_conn *c)
{
	//printf("I've sent!\n");
}

const static struct mesh_callbacks meshCallbacks = {mesh_recv, mesh_sent, mesh_timedout};
const static struct stbroadcast_callbacks stbroadcastCallbacks = {stbroadcast_recv, stbroadcast_sent};

static void
cancel_stbroadcast()
{
	printf("Canceling\n");
	stbroadcast_cancel(&stbroadcast);
}

static void
send_predicate_to_node(rimeaddr_t dest, char * pred)
{

	packetbuf_clear();
	packetbuf_set_datalen(strlen(pred));
	debug_packet_size(strlen(pred));
	packetbuf_copyfrom(pred, strlen(pred));

	mesh_send(&mesh, &dest); //send the message

}

static void 
send_to_base_station(char* message)
{
	memset(&baseStationAddr, 0, sizeof(rimeaddr_t));
	baseStationAddr.u8[sizeof(rimeaddr_t) - 2] = 1;

	packetbuf_clear();
	packetbuf_set_datalen(strlen(message));
	packetbuf_copyfrom(message, strlen(message));

	mesh_send(&mesh, &baseStationAddr); //send the message
}

static void
send_n_hop_predicate_check(rimeaddr_t originator, uint8_t message_id_to_send, char* pred, uint8_t hop_limit)
{

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(predicate_check_msg_t));
	debug_packet_size(sizeof(predicate_check_msg_t));
	predicate_check_msg_t * msg = (predicate_check_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(predicate_check_msg_t));
	
	msg->originator = rimeaddr_node_addr;
	msg->message_id = message_id_to_send;
	msg->predicate_to_check = pred;
	msg->hop_limit = hop_limit;

	random_init(rimeaddr_node_addr.u8[0]+2);
	int random = (random_rand() % 5);
	printf("random number = %d\n",random );
	stbroadcast_send_stubborn(&stbroadcast, random*CLOCK_SECOND);
	
	static struct ctimer stbroadcast_stop_timer;

	ctimer_set(&stbroadcast_stop_timer, 30 * CLOCK_SECOND, &cancel_stbroadcast, NULL);
}

PROCESS(networkInit, "Network Init");
PROCESS(mainProcess, "Main Predicate Checker Process");

AUTOSTART_PROCESSES(&networkInit, &mainProcess);

PROCESS_THREAD(networkInit, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	mesh_open(&mesh, 147, &meshCallbacks);

	//set the base station
	memset(&baseStationAddr, 0, sizeof(rimeaddr_t));
	baseStationAddr.u8[sizeof(rimeaddr_t) - 2] = 1;


	//5 second timer
	etimer_set(&et, 5 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	PROCESS_END();
}

PROCESS_THREAD(mainProcess, ev, data)
{
	//TODO: allocated instead of static
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	if (is_base()) //SINK
	{
		leds_on(LEDS_BLUE);

		while(1)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
	else //NODE
	{
		stbroadcast_open(&stbroadcast, 8, &stbroadcastCallbacks);

		list_init(message_list);

		etimer_set(&et, 20 * CLOCK_SECOND); //10 second timer

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		
		while(1)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			rimeaddr_t test;
			memset(&test, 0, sizeof(rimeaddr_t));
			test.u8[sizeof(rimeaddr_t) - 2] = 2;
			static int count = 0;
			if(rimeaddr_cmp(&rimeaddr_node_addr, &test) && count++ == 0)
			{
				message_id_received = get_message_id();
				send_n_hop_predicate_check(rimeaddr_node_addr, message_id_received, "Hello World!!!", 2);
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