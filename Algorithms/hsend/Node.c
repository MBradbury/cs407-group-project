#include "contiki.h"

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


PROCESS(messageSenderProcess, "Message Sender");
AUTOSTART_PROCESSES(&messageSenderProcess);

PROCESS_THREAD(messageSenderProcess, ev, data)
{
	PROCESS_BEGIN();
	while(1)
	{
		printf("Hello World\n");
	}
	PROCESS_END();
}