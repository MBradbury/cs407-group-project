#include "contiki.h"

#include "core/lib/list.h"

#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/stbroadcast.h"
#include ""

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;
static struct stbroadcast_conn stbroadcast;

static rimeaddr_t baseStationAddr;

static uint8_t message_id = 10;
static uint8_t message_id_received;


//Methods
static void 
send_n_hop_predicate_check(rimeaddr_t originator, uint8_t message_id, char* pred, uint8_t hop_limit);

static list_t message_list;

typedef struct 
{
	uint8_t message_id;
	uint8_t hops;
} message_elem_t;

typedef struct
{
	rimeaddr_t originator;
	uint8_t message_id;
	char* predicate_to_check;
	uint8_t hop_limit;
} predicate_check_msg_t;

bool is_base()
{
	static rimeaddr_t base;
	memset(&base, 0, sizeof(rimeaddr_t));
	base.u8[sizeof(rimeaddr_t) - 2] = 1;

	return rimeaddr_cmp(&rimeaddr_node_addr, &base) != 0;
}

/*static bool 
list_is_elem(list_t * list)
{
	while (list->next) 
	{
		
	}
}*/

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
		printf("Message received from: %s message: %s\n",addr2str(from),(char *)packetbuf_dataptr());
	}
	else
	{
		printf("Predicate received: %s from:%s \n",(char *)packetbuf_dataptr(),addr2str(from) );
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
  		printf("packet sent\n");
	}
}

static void
mesh_timedout(struct mesh_conn *c)
{
  	printf("packet timedout\n");

	if(is_base())
	{

	}
	else
	{

	}
}
	static uint8_t highest_hops_seen = 0;

static void
stbroadcast_recv(struct stbroadcast_conn *c)
{
	predicate_check_msg_t const * msg = (predicate_check_msg_t const *)packetbuf_dataptr();
	uint8_t hop_limit = msg->hop_limit;
	if (message_id_received != msg->message_id && hop_limit > highest_hops_seen) 
	{
		message_id_received = msg->message_id;
		highest_hops_seen = hop_limit;
		//send via mesh to originator
		printf("predicate: %s\n", msg->predicate_to_check);



		if (hop_limit > 1) //last node 
		{
			printf("Node resending: %s\n",addr2str(&rimeaddr_node_addr) );
			//send message on with one less hop limit
			send_n_hop_predicate_check(msg->originator,msg->message_id, msg->predicate_to_check,hop_limit - 1);
		}
	
		

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
sendPredicateToNode(rimeaddr_t dest, char * pred)
{
	packetbuf_clear();
	packetbuf_set_datalen(strlen(pred));
	debug_packet_size(strlen(pred));
	packetbuf_copyfrom(pred, strlen(pred));

	mesh_send(&mesh, &dest); //send the message

}

static void 
sendToBaseStation(char* message)
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

	stbroadcast_send_stubborn(&stbroadcast, CLOCK_SECOND);

	
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

	if (is_base())
	{
		while(1)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
	else
	{
		stbroadcast_open(&stbroadcast, 8, &stbroadcastCallbacks);
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
				send_n_hop_predicate_check(rimeaddr_node_addr,message_id_received, "Hello World!!!", 2);
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