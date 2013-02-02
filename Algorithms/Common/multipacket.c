#include "net/rime/multipacket.h"
#include "net/rime.h"
#include <string.h>
#include "linked-list.h"

static struct linked_list_t recvd_packets;
static int total_size = 0;
typedef struct { int seqno, void * data } list_packet_t;
static void * reconstructed;		//Need some way to get this back to the calling app

static const struct packetbuf_attrlist multipacket_attributes[] = {
	{ PACKETBUF_ATTR_EPACKET_ID, PACKETBUF_ATTR_EPACKET_TYPE },
	RUNICAST_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};

static void recv_from_runicast(struct runicast_conn *runicast, const rimeaddr_t *from)
{
	multipacket_conn * multipacket = (multipacket_conn *) runicast;
	multipacket->c = *runicast;
	int packet = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	int packets = packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE);
	printf("Received runicast from %d.%d; packet no. %d of %d\n", from->u8[0], from->u8[0], packet, packets);
	
	void * data = memcpy(data, packetbuf_dataptr(), packetbuf_datalen());
	
	struct list_packet_t * new_elem;
	new_elem->seqno = packet;
	new_elem->data = data;
	
	total_size += packetbuf_datalen();
	
	linked_list_append(&recvd_packets, new elem);
	
	if (linked_list_length(&recvd_packets)==packets)
	{
		int i = 1;
		while (i <= packets)
		{
			linked_list_elem_t elem;
			for (elem = linked_list_first(&recvd_packets); linked_list_continue(&recvd_packets, elem); elem = linked_list_next(elem))
			{
				struct list_packet_t * data = (list_packet_t *)linked_list_data(&list, elem);
				if (data->seqno==i)
				{
					memcpy(reconstructed+(i-1)*PACKETBUF_SIZE,data->data);
					break;
				}
			}
			i++;
		}
		multipacket->reconstructed_data = reconstructed;
		(*multipacket->callbacks.recv)(multipacket, originator);
	}
}

static void sent_by_runicast(struct runicast_conn *runicast, int status, int num_tx)
{
	int packet = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
	int packets = packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE);
	printf("Sent packet no. %d of %d\n", packet, packets);
}

static const struct runicast_callbacks multipacket = {recv_from_runicast, sent_by_runicast};

void multipacket_open(struct multipacket_conn *c, uint16_t channel,
					const struct multipacket_callbacks *u)
{
	runicast_open(&c->c, channel, &multipacket);
	channel_set_attributes(channel, multipacket_attributes);
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
	void * ptr  = c->original_data;
	
	int size = packetbuf_datalen();
	int packets = size/PACKETBUF_SIZE;
	
	if ( packets == 1 )
	{
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, 1);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, 1);
		runicast_send(c->c, receiver, max_rexmits);
	}
	else
	{
		for (int packet = 1; packet < packets; packet++)
		{
			packetbuf_clear();
			packetbuf_set_datalen(PACKETBUF_SIZE);
			debug_packet_size(PACKETBUF_SIZE);
			packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, packet);
			void * msg = packetbuf_dataptr();
			memcpy(msg, ptr, PACKETBUF_SIZE);
			
			//static struct ctimer ct;
			//ctimer_set(&ct, CLOCK_SECOND, &runicast_send, c->c, receiver, max_rexmits);
			runicast_send(c->c, receiver, max_rexmits);
			
			ptr += PACKETBUF_SIZE;
		}
			packetbuf_clear();
			packetbuf_set_datalen(size % PACKETBUF_SIZE);
			debug_packet_size(size % PACKETBUF_SIZE);
			packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, packet);		//Set seqno in header
			packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, packets);	//Set total no. packets in header
			void * msg = packetbuf_dataptr();
			memcpy(msg, ptr, size % PACKETBUF_SIZE);
			runicast_send(c->c, receiver, max_rexmits);
	}
	free(ptr);
}
