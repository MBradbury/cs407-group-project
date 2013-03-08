#ifndef CS407_MULTIPACKET_H
#define CS407_MULTIPACKET_H

#include "net/rime/runicast.h"

#include "containers/linked-list.h"
#include "containers/map.h"

#include <stdbool.h>
#include <string.h>

struct multipacket_conn;

typedef struct multipacket_callbacks
{
	void (* recv)(struct multipacket_conn * conn, rimeaddr_t const * from, void * data, size_t length);
	void (* sent)(struct multipacket_conn * conn, rimeaddr_t const * to, void * data, size_t length);
} multipacket_callbacks_t;

typedef struct multipacket_conn
{
	// Kepp connection in this order!
	struct runicast_conn rc;

	uint16_t id;
	
	linked_list_t sending_packets;
	map_t receiving_packets;
	
	multipacket_callbacks_t const * callbacks;

	struct ctimer ct_sender;

} multipacket_conn_t;

bool multipacket_open(multipacket_conn_t * conn, uint16_t channel, multipacket_callbacks_t const * callbacks);
void multipacket_close(multipacket_conn_t * conn);

void multipacket_send(multipacket_conn_t * conn, rimeaddr_t const * target, void * data, size_t length);

#endif /*CS407_MULTIPACKET_H*/
