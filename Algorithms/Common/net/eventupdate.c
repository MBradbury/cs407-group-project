#include "eventupdate.h"

#include <stdlib.h>
#include <stdio.h>

#include "debug-helper.h"
#include "random-range.h"

static void flood_recv(struct nhopflood_conn * c, rimeaddr_t const * source, uint8_t hops, uint8_t previous_hops)
{
	// Prevent delivering details about the current node
	// (that has been received via another node)
	if (!rimeaddr_cmp(source, &rimeaddr_node_addr))
	{
		event_update_conn_t * conn = (event_update_conn_t *)c;

		// Inform the client that an update has occured
		conn->update(conn, source, hops, previous_hops);
	}
}

static void data_check(void * p)
{
	event_update_conn_t * conn = (event_update_conn_t *)p;

	// Check if we should force sending
	double rnd = random_range_double(0, 1);
	bool force = rnd <= conn->chance;

	if (force)
	{
		printf("eup: Force up %d <= %d\n",
			(int)(rnd * 10000), (int)(conn->chance * 10000));
	}

	bool has_changed = false;

	// Check to see if we have any data currently stored
	if (conn->data_loc != NULL)
	{
		// Allocate some memory for the current data
		void * tmp = malloc(conn->data_size);
		conn->data_fn(tmp);

		has_changed = force || conn->differs_fn(conn->data_loc, tmp);

		// Data has changed, we are about to send it
		// so record the new data
		if (has_changed)
		{
			free(conn->data_loc);
			conn->data_loc = tmp;
		}
		else
		{
			free(tmp);
		}
	}
	else
	{
		// No data currently stored
		// so set the stored data to the recently gained data
		conn->data_loc = malloc(conn->data_size);
		conn->data_fn(conn->data_loc);

		has_changed = true;
	}


	// Data has changed so send update message
	if (has_changed)
	{
		unsigned int packet_size = conn->data_size;

		packetbuf_clear();
		packetbuf_set_datalen(packet_size);
		void * msg = packetbuf_dataptr();

		// Set the data to send
		memcpy(msg, conn->data_loc, conn->data_size);

		nhopflood_send(&conn->fc, conn->distance);
	}

	// Reset timer
	ctimer_reset(&conn->check_timer);
}


bool event_update_start(
	event_update_conn_t * conn, uint8_t ch, data_generation_fn data_fn,
	data_differs_fn differs_fn, size_t data_size, clock_time_t generate_period,
	update_fn update, float chance)
{
	if (conn != NULL && data_fn != NULL && data_size != 0 && generate_period != 0 && update != NULL)
	{
		nhopflood_start(&conn->fc, ch, &flood_recv, CLOCK_SECOND * 3, 3);

		conn->distance = 0;

		conn->data_fn = data_fn;
		conn->differs_fn = differs_fn;
		conn->data_size = data_size;
		conn->data_loc = NULL;
		conn->generate_period = generate_period;
		conn->update = update;
		conn->chance = chance;

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

		nhopflood_stop(&conn->fc);
	}
}

void event_update_set_distance(event_update_conn_t * conn, uint8_t distance)
{
	if (conn != NULL)
	{
		printf("eup: Set hops=%d\n", distance);

		conn->distance = distance;

		// Now that the distance has changed we need to trigger an update
		// the next chance we get. This is done by forgetting about the
		// last bit of data that we sent.
		free(conn->data_loc);
		conn->data_loc = NULL;
	}
}

