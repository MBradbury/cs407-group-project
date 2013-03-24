#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "dev/leds.h"

#include "net/rime/broadcast.h"

#include "containers/linked-list.h"
#include "containers/map.h"

#include "net/rimeaddr-helpers.h"
#include "debug-helper.h"

//
// See: arxiv.org/pdf/0808.0920v1.pdf
//

typedef struct
{
	unsigned int slot;
} msg_t;

typedef struct
{
	void * data;
	size_t length;
} queue_entry_t;

void free_queue_entry(void * ptr)
{
	queue_entry_t * entry = (queue_entry_t *)ptr;
	free(entry->data);
	free(entry);
}

typedef struct
{
	rimeaddr_t neighbour;
	unsigned int slot;

} neighbour_entry_t;


PROCESS(startup_process, "Startup");

AUTOSTART_PROCESSES(&startup_process);

// The slot this node has currently been assigned
static unsigned int assigned_slot = 0;

// List of queue_entry_t
static linked_list_t packet_queue;

// Map of neighbour_entry_t
static map_t neighbour_details;

// The maximum number of slots that can be assigned
// The lower this is the lower the latency
static const unsigned int slot_count = 10;

// The length of each slot
static const clock_time_t slot_length = 2 * CLOCK_SECOND;


static void recv(struct broadcast_conn * conn, rimeaddr_t const * sender)
{
	void * msg = packetbuf_dataptr();
	unsigned int length = packetbuf_datalen();
}

static struct broadcast_conn bc;
static const struct broadcast_callbacks callbacks = { &recv };


PROCESS_THREAD(startup_process, ev, data)
{
	static struct etimer et;
	static unsigned int current_slot = 0;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	broadcast_open(&bc, 126, &callbacks);

	linked_list_init(&packet_queue, &free_queue_entry);

	map_init(&neighbour_details, &rimeaddr_equality, &free);

	while (true)
	{
		etimer_set(&et, slot_length);

		// If this is our assigned slot, then we might broadcast
		if (current_slot == assigned_slot)
		{
			// Check that there is something to broadcast
			if (!linked_list_is_empty(&packet_queue))
			{
				// Get the first entry in the queue
				queue_entry_t * entry = (queue_entry_t *)linked_list_peek(&packet_queue);

				packetbuf_clear();
				packetbuf_set_datalen(entry->length);
				void * msg = packetbuf_dataptr();

				memcpy(msg, entry->data, entry->length);

				broadcast_send(&bc);

				// Remove the entry we just sent
				linked_list_pop(&packet_queue);
			}
		}

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		// Move to next slot
		current_slot += 1;
		current_slot %= slot_count;
	}

exit:
	map_free(&neighbour_details);
	linked_list_free(&packet_queue);
	broadcast_close(&bc);
	PROCESS_END();
}
