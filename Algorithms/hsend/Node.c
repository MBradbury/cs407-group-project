#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;
static rimeaddr_t dest;
static int messageSent = 0;

static bool isBase()
{
	rimeaddr_t base;
	memset(&base, 0, sizeof(rimeaddr_t));
	base.u8[sizeof(rimeaddr_t) - 2] = 1;

	return rimeaddr_cmp(&rimeaddr_node_addr, &base) == 1;
}

/** The function that will be executed when a message is received */
static void 
recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
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
sent(struct mesh_conn *c)
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
timedout(struct mesh_conn *c)
{
  	printf("packet timedout\n");

	if(isBase())
	{

	}
	else
	{

	}
}

const static struct mesh_callbacks callbacks = {recv, sent, timedout};

static void 
sendToBaseStation(char * message)
{
	packetbuf_clear();
	packetbuf_set_datalen(strlen(message));
	packetbuf_copyfrom(message, strlen(message));

	mesh_send(&mesh, &dest); //send the message
}

PROCESS(networkInit, "Network Init");
PROCESS(nodeProcess, "Node Process");
PROCESS(baseStation, "Sink Process");

AUTOSTART_PROCESSES(&networkInit);

PROCESS_THREAD(networkInit, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	mesh_open(&mesh, 147, &callbacks);

	//set the base station
	memset(&dest, 0, sizeof(rimeaddr_t));
	dest.u8[sizeof(rimeaddr_t) - 2] = 1;

	//5 second timer
	etimer_set(&et, 5 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	if (isBase())
	{
		process_start(&baseStation, NULL);
	}
	else
	{
		process_start(&nodeProcess, NULL);
	}

	PROCESS_END();
}

PROCESS_THREAD(nodeProcess, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	while(1)
	{
		etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

		if (messageSent == 0) 
		{
			char *message = "Hello World!!";
			sendToBaseStation(message);
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

	exit:
		printf("Exiting Node Process...\n");
		mesh_close(&mesh);
		PROCESS_END();
}

PROCESS_THREAD(baseStation, ev, data)
{
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	while(1)
	{

	}

	exit:
		printf("Exiting Base Process...\n");
		mesh_close(&mesh);
		PROCESS_END();
}	