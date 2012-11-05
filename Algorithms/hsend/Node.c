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
	//printf("Message received\n");

	//Print the message received
	printf("Message: %s\n",(char *)packetbuf_dataptr() );

	//TODO, don't broadcast when receiving own message
	if (rimeaddr_cmp(&rimeaddr_node_addr,originator) == 0)
	{
		printf("Sending received message\n");
		sendRIMEMessage("Message received");
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
		//open the connection, along with the callback function
		//TODO: some nodes can't be set to 1, as they will be the sink, Check with ID if ==1 become sink
		collect_set_sink(&tc, 1);
	
		packetbuf_clear(); //clear the buffer
		packetbuf_copyfrom(message, strlen(message)); //copy the message to the buffer
		collect_send(&tc, REXMITS); //send the buffer over tc
		//collect_close(&tc); //close the connection
}


PROCESS_THREAD(messageSenderProcess, ev, data)
{
	PROCESS_BEGIN();
	collect_open(&tc, 128, COLLECT_ROUTER, &callbacks);

	printf("Sending initial message\n"); //print to the log
	sendRIMEMessage("Hello World again!");

	PROCESS_END();
}

