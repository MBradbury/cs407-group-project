#include "contiki.h"

#include "net/rime.h"
#include "net/rime/mesh.h"

#include <stdio.h>

#include <string.h>

#include "lib/sensors.h"

#include "dev/sht11.h"
#include "dev/sht11-sensor.h"
#include "debug-helper.h"

static struct mesh_conn mesh;
static rimeaddr_t dest;

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
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	mesh_open(&mesh, 147, &callbacks);

	etimer_set(&et, 10 * CLOCK_SECOND);

	memset(&dest, 0, sizeof(rimeaddr_t));
	dest.u8[sizeof(rimeaddr_t) - 2] = 1;

	while(1) 
	{
	    char * msg = "This is a message";

		packetbuf_clear();
		packetbuf_set_datalen(strlen(msg));
		packetbuf_copyfrom(msg, strlen(msg)); 	
		//memset(msg, 0, strlen(msg));

			if (rimeaddr_cmp(&rimeaddr_node_addr,&dest) == 0) 
			{
				int result = mesh_send(&mesh, &dest);

		    	printf("Sending a message: %d to: %s\n", result, addr2str(&dest));
			}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

 exit:
	printf("Exiting...\n");
	mesh_close(&mesh);
	PROCESS_END();
}