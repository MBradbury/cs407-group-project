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

	printf("%s\n",(char *)packetbuf_dataptr() );
}	

/** List of all functions to execute when a message is received */
static const struct collect_callbacks callbacks = { recv };

static struct collect_conn tc;
static const int REXMITS = 4;

PROCESS(messageSenderProcess, "Message Sender");
AUTOSTART_PROCESSES(&messageSenderProcess);

PROCESS_THREAD(messageSenderProcess, ev, data)
{
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();
	

	while(1)
	{
		//printf("Hello World\n");
		collect_open(&tc, 128, COLLECT_ROUTER, &callbacks);
	
	if (rimeaddr_node_addr.u8[0] == 1 &&
		rimeaddr_node_addr.u8[1] == 0)
	{
		char * msg = "Hello World!";

		collect_set_sink(&tc, 1);
	
		//packetbuf_clear();
		packetbuf_copyfrom(msg, 6);
		collect_send(&tc, REXMITS);
	}
	}
exit:
	collect_close(&tc);
	PROCESS_END();}