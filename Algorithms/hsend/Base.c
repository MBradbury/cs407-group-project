#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>
#include <string.h>

#include "debug-helper.h"

static struct mesh_conn mesh;
static rimeaddr_t dest;


/** The function that will be executed when a message is received */
static void 
recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)

{
	printf("Message received from: %s hops: %d message: %s\n",addr2str(from),hops,(char *)packetbuf_dataptr());
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

const static struct mesh_callbacks callbacks = {recv, sent, timedout};


PROCESS(baseStation, "Base Station");
AUTOSTART_PROCESSES(&baseStation);

PROCESS_THREAD(baseStation, ev, data)
{
	//PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	mesh_open(&mesh, 147, &callbacks);

	//set the base station to self
	memset(&dest, 0, sizeof(rimeaddr_t));
	dest.u8[sizeof(rimeaddr_t) - 2] = 1;
	rimeaddr_set_node_addr(&dest);

	/*
	exit:
		printf("Exiting...\n");
		mesh_close(&mesh);
		*/
		PROCESS_END();

}