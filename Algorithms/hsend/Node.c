#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;
static rimeaddr_t dest;

const static struct mesh_callbacks callbacks = {recv, sent, timedout};

/** The function that will be executed when a message is received */
static void 
recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)

{
	printf("Message received from: %d.%d message: %s\n",from->u8[0],from->u8[1],(char *)packetbuf_dataptr());
}	

static void
sent(struct mesh_conn *c)
{
  printf("packet sent\n");
}
static void
timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");
}

static void 
sendToBaseStation(char * message)
{

}

PROCESS(messageSenderProcess, "Message Sender");
AUTOSTART_PROCESSES(&messageSenderProcess);

PROCESS_THREAD(messageSenderProcess, ev, data)
{



}	