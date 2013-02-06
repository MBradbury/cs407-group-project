#include "net/rime.h"
#include "net/rime/packetqueue.h"
#include "net/rime/broadcast.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	struct broadcast_conn bc;

	unsigned int message_id;
	unsigned int data_size;
	unsigned int max_hops;

	data_generation_fn data_fn;
	data_receive_fn receive_fn;

	clock_time_t send_delay;
	clock_time_t period;
	clock_time_t max;

	struct ctimer delay_timer;
	struct ctimer send_timer;
	struct ctimer cancel_timer;
} nhopflood_conn_t;

// Initialise n-hop data flooding.
bool nhopflood_start(
	nhopflood_conn_t * conn, uint8_t ch1, data_generation_fn data_fn, 
	data_receive_fn receive_fn, unsigned int data_size);

// Shutdown n-hop data flooding.
bool nhopflood_end(nhopflood_conn_t * conn);

// Send an n-hop data flood.
void nhopflood_send(nhopflood_conn_t * conn, uint8_t hops);
