#ifndef CS407_NHOPREQ_H
#define CS407_NHOPREQ_H

#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"

#include "map.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*data_generation_fn)(void * data);

typedef void (*data_receive_fn)(rimeaddr_t const * from, uint8_t hops, void const * data);

typedef struct
{
	struct runicast_conn ru;
	struct stbroadcast_conn bc;

	rimeaddr_t base_station_addr;

	uint16_t message_id;

	unsigned int data_size;
	data_generation_fn data_fn;
	data_receive_fn receive_fn;

	map_t messages;

} nhopreq_conn_t;

bool is_base_station(nhopreq_conn_t const * conn);

// Initialise multi-hop predicate checking
bool nhopreq_start(
	nhopreq_conn_t * conn, uint8_t ch1, uint8_t ch2, rimeaddr_t const * baseStationAddr,
	data_generation_fn data_fn, unsigned int data_size, data_receive_fn receive_fn);

// Shutdown multi-hop predicate checking
bool nhopreq_end(nhopreq_conn_t * conn);

void nhopreq_request_info(nhopreq_conn_t * conn, uint8_t hops);

#endif /*CS407_NHOPREQ_H*/

