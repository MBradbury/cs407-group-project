#ifndef CS407_HSEND_H
#define CS407_HSEND_H

#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"

#include "lib/list.h"

#include <stdbool.h>
#include <stdint.h>

typedef void (*data_generation_fn)(void * data);

typedef void (*data_receive_fn)(rimeaddr_t const * from, void const * data);

typedef struct
{
	struct runicast_conn ru;
	struct stbroadcast_conn bc;

	rimeaddr_t baseStationAddr;

	uint8_t message_id;

	unsigned int data_size;
	data_generation_fn data_fn;
	data_receive_fn receive_fn;

	LIST_STRUCT(message_list);

} hsend_conn_t;

bool is_base(hsend_conn_t const * conn);

// Initialise multi-hop predicate checking
bool hsend_start(
	hsend_conn_t * conn, uint8_t ch1, uint8_t ch2, rimeaddr_t const * baseStationAddr,
	data_generation_fn data_fn, unsigned int data_size, data_receive_fn receive_fn);

// Shutdown multi-hop predicate checking
bool hsend_end(hsend_conn_t * conn);

unsigned int hsend_request_info(unsigned int hops, void ** data);

#endif /*CS407_HSEND_H*/

