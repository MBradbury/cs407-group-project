#ifndef CS407_NHOPREQ_H
#define CS407_NHOPREQ_H

#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"

#include "containers/map.h"

#include <stdbool.h>
#include <stdint.h>


struct nhopreq_conn;

typedef struct
{
	void (* data_fn)(struct nhopreq_conn * conn, void * data);

	void (* receive_fn)(struct nhopreq_conn * conn, rimeaddr_t const * from, uint8_t hops, void const * data);

} nhopreq_callbacks_t;

typedef struct nhopreq_conn
{
	struct runicast_conn ru;
	struct stbroadcast_conn bc;

	uint16_t message_id;

	unsigned int data_size;

	nhopreq_callbacks_t const * callbacks;

	map_t mote_records;

	struct ctimer runicast_timer;
	struct ctimer forward_timer;
	struct ctimer datareq_stbroadcast_stop_timer;

} nhopreq_conn_t;

// Initialise multi-hop predicate checking
bool nhopreq_start(
	nhopreq_conn_t * conn, uint8_t ch1, uint8_t ch2,
	unsigned int data_size, nhopreq_callbacks_t const * callbacks);

// Shutdown multi-hop predicate checking
bool nhopreq_stop(nhopreq_conn_t * conn);

void nhopreq_request_info(nhopreq_conn_t * conn, uint8_t hops);

#endif /*CS407_NHOPREQ_H*/
