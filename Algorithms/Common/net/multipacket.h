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
	void (* recv)(struct multipacket_conn * conn,
		rimeaddr_t const * from, void * data, unsigned int length);
	void (* sent)(struct multipacket_conn * conn,
		rimeaddr_t const * to, void * data, unsigned int length);
} multipacket_callbacks_t;

typedef struct multipacket_conn
{
	// Keep connection in this order!
	struct runicast_conn rc;

	uint16_t id;
	
	linked_list_t sending_packets; // A queue of packets to be sent
	map_t receiving_packets; // Map of recv_key_t to multipacket_receiving_packet_t
	
	multipacket_callbacks_t const * callbacks;

	struct ctimer ct_sender;

} multipacket_conn_t;

bool multipacket_open(multipacket_conn_t * conn,
	uint16_t channel, multipacket_callbacks_t const * callbacks);
void multipacket_close(multipacket_conn_t * conn);

void multipacket_send(multipacket_conn_t * conn,
	rimeaddr_t const * target, void * data, unsigned int length);

#endif /*CS407_MULTIPACKET_H*/
