#include "contiki.h"

#include "dev/leds.h"
#include "lib/list.h"

#include "net/rime.h"
#include "net/rime/neighbor-discovery.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "debug-helper.h"

static rimeaddr_t base_station_rimeaddr; //address of the base station
static struct neighbor_discovery_conn neighbor_discovery; //Neighbour Discovery Protocol Connection

struct neighbor_list_item
{
	struct neighbor_list_item *next;
	rimeaddr_t neighbor_rimeaddr;
};