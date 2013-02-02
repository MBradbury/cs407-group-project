#ifndef __MULTIPACKET_H__
#define __MULTIPACKET_H__

#include "net/rime/runicast.h"

struct multipacket_conn;

typedef struct {
	void (* recv)(struct multipacket_conn *c, const rimeaddr_t *from);
	void (* sent)(struct multipacket_conn *c, const rimeaddr_t *to, uint8_t retransmissions);
	void (* timedout)(struct multipacket_conn *c, const rimeaddr_t *to, uint8_t retransmissions);
} multipacket_callbacks_t;

struct multipacket_conn {
	struct runicast_conn c;
	
	void * original_data;
	void * reconstructed_data;
	size_t length;
	
	multipacket_callbacks_t callbacks;
};

void multipacket_open(struct multipacket_conn *c, uint16_t channel,
					const struct multipacket_callbacks *u);

void multipacket_close(struct multipacket_conn *c);

int multipacket_send(struct multipacket_conn *c, const rimeaddr_t *receiver,
					uint8_t max_retransmissions);

uint8_t multipacket_is_transmitting(struct runicast_conn *c);

#endif /* __MULTIPACKET_H__ */
