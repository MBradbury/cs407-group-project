#include "eventupdate.h"


static const clock_time_t RETX_INTERVAL = 2 * CLOCK_SECOND;


// The custom headers we use
static const struct packetbuf_attrlist trickle_attributes[] = {
	{ PACKETBUF_ADDR_ESENDER, PACKETBUF_ADDRSIZE },
	{ PACKETBUF_ATTR_HOPS, PACKETBUF_ATTR_BIT * 4 },
	{ PACKETBUF_ATTR_TTL, PACKETBUF_ATTR_BIT * 4 },
	BROADCAST_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};



static void sb_recv(struct stbroadcast_conn * c)
{
	event_update_conn_t * conn = (event_update_conn_t *)c;

	// TODO: Forward the data onwards depending on the TTL

	// Inform the client that an update has occured
	conn->update_fn(
		packetbuf_addr(PACKETBUF_ADDR_ESENDER),
		packetbuf_attr(PACKETBUF_ATTR_HOPS),
		packetbuf_dataptr()
		);
}

static void sb_sent(struct stbroadcast_conn * c)
{
}


static const struct stbroadcast_callbacks callbacks = {&sb_recv, &sb_sent};




static void data_check(void * p)
{
	event_update_conn_t * conn = (event_update_conn_t *)p;

	bool has_changed = false;

	void * tmp = malloc(conn->data_size);
	conn->data_fn(tmp);

	if (conn->data_loc != NULL)
	{
		has_changed = conn->differs_fn(conn->data_loc, tmp);

		// Data has changed, we are about to send it
		// so record the new data
		if (has_changed)
		{
			void * swap = tmp;
			tmp = conn->data_loc;
			conn->data_loc = swap;
		}
	}
	else
	{
		conn->data_loc = tmp;
		tmp = NULL;

		has_changed = true;
	}

	free(tmp);
	tmp = NULL;


	// Data has changed so send update message
	if (has_changed)
	{
		unsigned int packet_size = conn->data_size;

		packetbuf_clear();
		packetbuf_set_datalen(packet_size);
		debug_packet_size(packet_size);
		void * msg = packetbuf_dataptr();
		memset(msg, 0, packet_size);

		// Set the source of this messages
		packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &rimeaddr_node_addr);

		// Set the TTL
		packetbuf_set_attr(PACKETBUF_ATTR_TTL, conn->distance);

		// Set the number of hops traveled
		packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 1);

		// Set the data to send
		memcpy(msg, conn->data_loc, conn->data_size);

		// TODO: change this to use message buffers!!
		// TODO: or at least have the stbroadcast stop sending at some point

		stbroadcast_send(&conn->sc, RETX_INTERVAL);
	}

	// Reset timer
	ctimer_reset(&conn->check_timer);
}


bool event_update_start(event_update_conn_t * conn, uint8_t ch, data_generation_fn data_fn, data_differs_fn differs_fn, size_t data_size, clock_time_t generate_period, update_fn update)
{
	if (conn != NULL && data_fn != NULL && data_size != 0 && generate_period != 0 && update != NULL)
	{
		stbroadcast_open(&conn->sc, ch, &callbacks);

		conn->distance = 0;

		conn->data_fn = data_fn;
		conn->differs_fn = differs_fn;
		conn->data_size = data_size;
		conn->data_loc = NULL;
		conn->generate_period = generate_period;
		conn->update = update;

		ctimer_set(&conn->check_timer, generate_period, &data_check, conn);

		return true;
	}
	return false;
}

void event_update_stop(event_update_conn_t * conn)
{
	if (conn != NULL)
	{
		ctimer_stop(&conn->check_timer);

		free(conn->data_loc);
		conn->data_loc = NULL;

		stdbroadcast_close(&conn->sc);
	}
}

void event_update_set_distance(event_update_conn_t * conn, uint8_t distance)
{
	if (conn != NULL)
	{
		conn->distance = distance;
	}
}
