#ifndef CS407_NHOPFLOOD
#define CS407_NHOPFLOOD

#include "sys/ctimer.h"
#include "net/rime.h"
#include "net/rime/broadcast.h"

#include "linked-list.h"
#include "map.h"

#include <stdbool.h>
#include <stdint.h>

struct nhopflood_conn;

typedef void (*nhopflood_recv_fn)(struct nhopflood_conn * conn, rimeaddr_t const * source, uint8_t hops, uint8_t previous_hops);

typedef struct nhopflood_conn
{
	struct broadcast_conn bc;

	nhopflood_recv_fn receive_fn;

	uint8_t current_id;

	// Maximum number of retransmits
	uint8_t maxrx;

	clock_time_t send_period;

	struct ctimer send_timer;

	linked_list_t packet_queue;
	map_t latest_message_seen;
	
} nhopflood_conn_t;

// Initialise n-hop data flooding.
bool nhopflood_start(nhopflood_conn_t * conn, uint8_t ch, nhopflood_recv_fn receive_fn,
	clock_time_t send_period, uint8_t maxrx);

// Shutdown n-hop data flooding.
void nhopflood_stop(nhopflood_conn_t * conn);

// Send an n-hop data flood.
bool nhopflood_send(nhopflood_conn_t * conn, uint8_t hops);

#endif /*CS407_NHOPFLOOD*/