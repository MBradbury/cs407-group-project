#include <stdio.h>

#include "lib/sensors.h"

#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "net/rime/collect-neighbor.h"
#include "net/rime/timesynch.h"
#include "contiki-net.h"

/** The structure of the message we are sending */
typedef struct collect_msg
{
	double temperature;
	double humidity;

} collect_msg_t;

static const struct collect_callbacks callbacks = { recv };

static void recv(rimeaddr_t const * originator, uint8_t seqno, uint8_t hops)
{
	collect_msg_t const * msg;

	msg = (collect_msg_t const *)packetbuf_dataptr();

	printf("Network Data: Addr:%d.%d Seqno:%u Hops:%u Temp:%d Hudmid:%d%%\n",
		originator->u8[0], originator->u8[1],
		seqno, hops,
		(int)msg->temperature, (int)msg->humidity
	);
}


PROCESS(hsendBaseProcess, "HSEND Base");
AUTOSTART_PROCESSES(&hsendBaseProcess);

PROCESS_THREAD(hsendBaseProcess, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();
	
	etimer_set(&et, CLOCK_SECOND);

	while(1)
	{

		PROCESS_YIELD();	



		etimer_reset(&et);
	}

	exit:
	collect_close(&tc);
	PROCESS_END();
}

static void recv(rimeaddr_t const * originator, uint8_t seqno, uint8_t hops)
{

	printf("%s\n",(char *)packetbuf_dataptr() );
}