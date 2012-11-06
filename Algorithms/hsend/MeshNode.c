#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>

#include <string.h>

#include "lib/sensors.h"

#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

static struct mesh_conn mesh;

/** The function that will be executed when a message is received */
static void recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)

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

const static struct mesh_callbacks callbacks = {recv, sent, timedout};

PROCESS(messageSenderProcess, "Message Sender");
AUTOSTART_PROCESSES(&messageSenderProcess);

PROCESS_THREAD(messageSenderProcess, ev, data)
{
	//PROCESS_EXITHANDLER(mesh_close(&mesh);)
	PROCESS_BEGIN();

	mesh_open(&mesh, 132, &callbacks);

	rimeaddr_t dest;
	dest.u8[0] = 1;
    dest.u8[1] = 0;

	//while(1) {


    char * msg = "This is a message";

	packetbuf_clear();
	packetbuf_set_datalen(strlen(msg));
	packetbuf_copyfrom(msg, strlen(msg)); 	
	//memset(msg, 0, strlen(msg));

		if (rimeaddr_cmp(&rimeaddr_node_addr,&dest) == 0) {
	    printf("Sending a message: %d to: %d.%d\n",mesh_send(&mesh, &dest),dest.u8[0],dest.u8[1]);
	}

 //}
	PROCESS_END();
}