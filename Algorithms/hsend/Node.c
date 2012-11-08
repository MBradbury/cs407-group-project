#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/runicast.h"

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;
static struct runicast_conn runicast;

static rimeaddr_t dest;
static int messageSent = 0;
static bool predicate;

typedef struct
{
	rimeaddr_t originator;
	uint8_t message_id;
	char* predicate_to_check;
	uint8_t hop_limit;
} predicate_check_msg_t;

bool isBase()
{
	rimeaddr_t base;
	memset(&base, 0, sizeof(rimeaddr_t));
	base.u8[sizeof(rimeaddr_t) - 2] = 1;

	return rimeaddr_cmp(&rimeaddr_node_addr, &base) != 0;
}

/** The function that will be executed when a message is received */
static void 
mesh_recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
	if(isBase())
	{
		printf("Message received from: %s message: %s\n",addr2str(from),(char *)packetbuf_dataptr());
	}
	else
	{

	}
}	

static void
mesh_sent(struct mesh_conn *c)
{
	if(isBase())
	{

	}
	else
	{
		messageSent = 1;
  		printf("packet sent\n");
	}
}

static void
mesh_timedout(struct mesh_conn *c)
{
  	printf("packet timedout\n");

	if(isBase())
	{

	}
	else
	{

	}
}

static void
runicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
	if(isBase())
	{

	}
	else
	{

	}
}

static void
runicast_sent(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	if(isBase())
	{

	}
	else
	{

	}
}

static void
runicast_timedout(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	if(isBase())
	{

	}
	else
	{

	}
}

const static struct mesh_callbacks meshCallbacks = {mesh_recv, mesh_sent, mesh_timedout};
const static struct runicast_callbacks runicastCallbacks = {runicast_recv, runicast_sent, runicast_timedout};


static void 
sendToBaseStation(char* message)
{
	packetbuf_clear();
	packetbuf_set_datalen(strlen(message));
	packetbuf_copyfrom(message, strlen(message));

	mesh_send(&mesh, &dest); //send the message
}

static void
sendNHopPredicateCheck()
{
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(predicate_check_msg_t));
	debug_packet_size(sizeof(predicate_check_msg_t));
	predicate_check_msg_t * msg = (predicate_check_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(predicate_check_msg_t));

	msg->originator = ;
	msg->message_id = ;
	msg->predicate_to_check = ;
	msg->hop_limit = ;
	
	runicast_send(runicast)

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
	memset(&dest, 0, sizeof(rimeaddr_t));
	dest.u8[sizeof(rimeaddr_t) - 2] = 1;

	//Set the predicate value
	predicate = true;

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

	if (isBase())
	{
		while(1)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
	else
	{
		runicast_open(&runicast, 2, &runicastCallbacks);

		while(1)
		{
			etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

			rimeaddr_t test;
			memset(&test, 0, sizeof(rimeaddr_t));
			test.u8[sizeof(rimeaddr_t) - 2] = 2;

			if(rimeaddr_cmp(&rimeaddr_node_addr, &test) != 0)
			{

			}

			if (messageSent == 0) 
			{
				char *message = "Hello World!!";
				sendToBaseStation(message);
			}

			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}

	exit:
		printf("Exiting Base Process...\n");
		mesh_close(&mesh);
		PROCESS_END();
}