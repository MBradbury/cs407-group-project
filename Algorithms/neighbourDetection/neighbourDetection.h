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

struct neighbor_list_item_t
{
	struct neighbor_list_item_t *next;
	rimeaddr_t neighbor;
};

LIST(neighbor_list); //create neighbour list 

static rimeaddr_t baseStationAddr; //address of the base station
static struct neighbor_discovery_conn neighborDiscovery; //Neighbour Discovery Protocol Connection

static void
neighbor_discovery_recv(struct neighbor_discovery_conn * c, const rimeaddr_t * from, uint16_t val);

static void
neighbor_discovery_sent(struct neighbor_discovery_conn * c);

static const struct neighbor_discovery_callbacks neighborDiscoveryCallbacks = {neighbor_discovery_recv, neighbor_discovery_sent};