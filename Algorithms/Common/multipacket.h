#ifndef __MULTIPACKET_H__
#define __MULTIPACKET_H__

#include "net/rime/runicast.h"

struct multipacket_conn;

#define RUNICAST_PACKET_ID_BITS 2

#define RUNICAST_ATTRIBUTES { PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_BIT }, \
							{ PACKETBUF_ATTR_PACKET_ID, PACKETBUF_ATTR_BIT * RUNICAST_PACKET_ID_BITS }, \
							STUNICAST_ATTRIBUTES
struct runicast_callbacks {
	void (* recv)(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno);
	void (* sent)(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions);
	void (* timedout)(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions);
};

struct multipacket_conn {
	struct runicast_conn c;
	const struct multipacket_callbacks *u;
};

void multipacket_open(struct multipacket_conn *c, uint16_t channel,
					const struct multipacket_callbacks *u);

void multipacket_close(struct multipacket_conn *c);

int multipacket_send(struct multipacket_conn *c, const rimeaddr_t *receiver,
					uint8_t max_retransmissions);

uint8_t multipacket_is_transmitting(struct runicast_conn *c);

#endif /* __MULTIPACKET_H__ */
