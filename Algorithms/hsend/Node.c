#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;
static rimeaddr_t dest;
static int messageSent = 0;

/** The function that will be executed when a message is received */
static void 
recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)

{
	printf("Message received from: %s message: %s\n",addr2str(from),(char *)packetbuf_dataptr());
}	

static void
sent(struct mesh_conn *c)
{
	messageSent = 1;
  printf("packet sent\n");
}
static void
timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");
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

PROCESS(messageSenderProcess, "Message Sender");
AUTOSTART_PROCESSES(&messageSenderProcess);

PROCESS_THREAD(messageSenderProcess, ev, data)
{
	static struct etimer et;



	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	mesh_open(&mesh, 147, &callbacks);

	//set the base station
	memset(&dest, 0, sizeof(rimeaddr_t));
	dest.u8[sizeof(rimeaddr_t) - 2] = 1;

	etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

	while(1)
	{
		if (messageSent == 0) {
		char *message = "Hello World!!";
		sendToBaseStation(message);
		}
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

	}

	exit:
		printf("Exiting...\n");
		mesh_close(&mesh);
		PROCESS_END();
}	