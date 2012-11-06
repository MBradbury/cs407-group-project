#include "contiki.h"

#include <stdio.h>

#include <string.h>

#include "lib/sensors.h"

#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"

#include "net/rime/collect.h"
#include "net/rime/collect-neighbor.h"
#include "net/rime/timesynch.h"
#include "contiki-net.h"



/** The function that will be executed when a message is received */
static void recv(rimeaddr_t const * originator, uint8_t seqno, uint8_t hops)
{
	//if message was from another node, print it out
	if (rimeaddr_cmp(&rimeaddr_node_addr,originator) == 0)
	{
		printf("Message received from %d: %s \n",address->u8[0],(char *)packetbuf_dataptr() );
	}
}	

/** List of all functions to execute when a message is received */
static const struct collect_callbacks callbacks = { recv };

static struct collect_conn tc;
static const int REXMITS = 4;

PROCESS(messageSenderProcess, "Message Sender");
AUTOSTART_PROCESSES(&messageSenderProcess);

/* Method will broadcast a message using RIME */
void sendRIMEMessage(char * message)
{
		packetbuf_clear(); //clear the buffer
		packetbuf_set_datalen(sizeof(message));

		packetbuf_copyfrom(message, strlen(message)); //copy the message to the buffer
		collect_send(&tc, REXMITS); //send the buffer over tc
}


PROCESS_THREAD(messageSenderProcess, ev, data)
{
	PROCESS_BEGIN();

	//open the connection, along with the callback function
	collect_open(&tc, 128, COLLECT_ROUTER, &callbacks);
	
	//if the address of the node is 1, set to the sink
	if (rimeaddr_node_addr.u8[0] == 1 &&
	rimeaddr_node_addr.u8[1] == 0)
	{
		collect_set_sink(&tc, 1);
	}
	else 
	{
		collect_set_sink(&tc,0);
	}

	//if the sink node, seend Hello World!
	if (rimeaddr_node_addr.u8[0] == 1)
	{
		sendRIMEMessage("Hello World!");

	}
	else 
	{
		sendRIMEMessage("Hello World again!");
	}

	PROCESS_END();
}

