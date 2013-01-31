#include "net/rime/multipacket.h"
#include "net/rime.h"
#include <string.h>

static void recv_from_runicast(struct runicast_conn *runicast, const rimeaddr_t *from)
{

}

static void sent_by_runicast(struct runicast_conn *runicast, int status, int num_tx)
{

}

static const struct runicast_callbacks multipacket = {recv_from_runicast, sent_by_runicast};

void multipacket_open(struct multipacket_conn *c, uint16_t channel,
					const struct multipacket_callbacks *u)
{
	runicast_open(&c->c, channel, &runicast);
	channel_set_attributes(channel, attributes);
	c->c->u = u;
	c->c->is_tx = 0;
	c->c->rxmit = 0;
	c->c->sndnxt = 0;
}

void multipacket_close(struct multipacket_conn *c)
{
	runicast_close(&c->c);
}

uint8_t multipacket_is_transmitting(struct multipacket_conn *c)
{
	return runicast_is_transmitting(c->c);
}

int multipacket_send(struct multipacket_conn *c, const rimeaddr_t *receiver, uint8_t max_rexmits)
{
	runicast_send(c->c, receiver, max_rexmits);
}
