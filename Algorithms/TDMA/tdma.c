#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "sys/ctimer.h"

#include "dev/leds.h"

#include "net/rime/broadcast.h"

#include "containers/linked-list.h"
#include "containers/map.h"

#include "net/rimeaddr-helpers.h"
#include "debug-helper.h"

//
// See: arxiv.org/pdf/0808.0920v1.pdf
//

static const struct packetbuf_attrlist attributes[] = {
	{ PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_BYTE * sizeof(uint8_t) },	// Type
	BROADCAST_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};


typedef struct
{
	unsigned int length;
	bool is_change;
} msg_t;


#define PACKET_TYPE_USER 0
#define PACKET_TYPE_PROTOCOL_BEACON 1

typedef struct
{
	void * data;
	size_t length;
	uint8_t type;
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
static unsigned int to_change_assigned_slot;

// List of queue_entry_t
static linked_list_t packet_queue;

// Map of neighbour_entry_t
static map_t neighbour_details;

// List of rimeaddr_t
static unique_array_t one_hop_neighbours;

// The maximum number of slots that can be assigned
// The lower this is the lower the latency
static const unsigned int slot_count = 10;

// The length of each slot
static const clock_time_t slot_length = 2 * CLOCK_SECOND;

// The length of each round
static const clock_time_t round_length = 10 * CLOCK_SECOND;

static struct ctimer ct_change_assigned_slot;

static inline bool rimeaddr_lt(rimeaddr_t const * left, rimeaddr_t const * right)
{
	return (*(uint16_t const *)left) < (*(uint16_t const *)right);
}

static void bcast(struct broadcast_conn * conn, void * data, size_t length, uint8_t type)
{
	packetbuf_clear();
	packetbuf_set_datalen(length);
	void * msg = packetbuf_dataptr();

	memcpy(msg, data, length);

	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, type);

	broadcast_send(conn);
}

static void bcast_beacon(struct broadcast_conn * conn, bool is_change)
{
	unsigned int length = unique_array_length(&one_hop_neighbours) + 1;
	unsigned int packet_length = sizeof(msg_t) + (sizeof(neighbour_entry_t) * length);

	packetbuf_clear();
	packetbuf_set_datalen(packet_length);
	msg_t * msg = (msg_t *)packetbuf_dataptr();

	msg->length = length;
	msg->is_change = is_change;

	neighbour_entry_t * entries = (neighbour_entry_t *)(msg + 1);

	// Copy in our value
	rimeaddr_copy(&entries->neighbour, &rimeaddr_node_addr);
	entries->slot = assigned_slot;

	++entries;

	// Copy in the data from our one hop neighbourhood
	unique_array_elem_t elem;
	for (elem = unique_array_first(&one_hop_neighbours);
		 unique_array_continue(&one_hop_neighbours, elem);
		 elem = unique_array_next(elem))
	{
		rimeaddr_t const * neighbour =
			(rimeaddr_t const *)unique_array_data(&one_hop_neighbours, elem);

		void * data = map_get(&neighbour_details, neighbour);

		memcpy(entries, data, sizeof(neighbour_entry_t));

		++entries;
	}

	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKET_TYPE_PROTOCOL_BEACON);

	broadcast_send(conn);
}

static void set_slot(void * ptr)
{
	struct broadcast_conn * conn = (struct broadcast_conn *)ptr;

	printf("Set slot to %u from %u\n", to_change_assigned_slot, assigned_slot);

	assigned_slot = to_change_assigned_slot;

	bcast_beacon(conn, true);
}

static void recv(struct broadcast_conn * conn, rimeaddr_t const * sender)
{
	void * ptr = packetbuf_dataptr();
	unsigned int length = packetbuf_datalen();

	uint8_t type = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE);

	// Record who is in our one hop neighbourhood
	if (!unique_array_contains(&one_hop_neighbours, sender))
	{
		unique_array_append(&one_hop_neighbours, rimeaddr_clone(sender));
	}

	switch (type)
	{
	case PACKET_TYPE_USER:
		{
		} break;

	case PACKET_TYPE_PROTOCOL_BEACON:
		{
			msg_t * msg = (msg_t *)ptr;
			neighbour_entry_t const * entries = (neighbour_entry_t const *)(msg + 1);

			printf("Received beacon from %s length=%u change=%u\n",
				addr2str(sender), msg->length, msg->is_change);

			// Cancel, if a node with a lower id is also changing
			if (msg->is_change && rimeaddr_lt(sender, &rimeaddr_node_addr))
			{
				if (ctimer_expired(&ct_change_assigned_slot) != 0)
				{
					printf("Stopping change slot timer\n");

					ctimer_stop(&ct_change_assigned_slot);
				}
			}

			unsigned int i;
			for (i = 0; i != msg->length; ++i)
			{
				// Don't record information on self
				if (!rimeaddr_cmp(&entries[i].neighbour, &rimeaddr_node_addr))
				{
					// Update or store information on this node
					neighbour_entry_t * entry = map_get(&neighbour_details, &entries[i].neighbour);

					if (entry == NULL)
					{
						entry = malloc(sizeof(neighbour_entry_t));
						memcpy(entry, &entries[i], sizeof(sizeof(neighbour_entry_t)));

						map_put(&neighbour_details, entry);
					}
					else
					{
						entry->slot = entries[i].slot;
					}
				}
			}

			printf("Searching for better slot...\n");

			// Find the smallest slot not in use by other nodes
			for (i = 0; i != slot_count; ++i)
			{
				//printf("Checking slot %u\n", i);

				bool in_use = false;

				map_elem_t elem;
				for (elem = map_first(&neighbour_details);
					 map_continue(&neighbour_details, elem);
					 elem = map_next(elem))
				{
					neighbour_entry_t const * entry =
						(neighbour_entry_t const *)map_data(&neighbour_details, elem);

					if (entry->slot == i)
					{
						in_use = true;
						break;
					}
				}

				if (!in_use)
				{
					if (i != assigned_slot)
					{
						printf("Found a better slot of %u compared to %u\n", i, assigned_slot);

						to_change_assigned_slot = i;
						ctimer_set(&ct_change_assigned_slot, round_length / 2, &set_slot, conn);
					}

					break;
				}
			}


		} break;
	}
}

static struct broadcast_conn bc;
static const struct broadcast_callbacks callbacks = { &recv };


PROCESS_THREAD(startup_process, ev, data)
{
	static struct etimer et_tdma, et_round;
	static unsigned int current_slot = 0;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	linked_list_init(&packet_queue, &free_queue_entry);
	map_init(&neighbour_details, &rimeaddr_equality, &free);
	unique_array_init(&one_hop_neighbours, &rimeaddr_equality, &free);

	broadcast_open(&bc, 126, &callbacks);
	channel_set_attributes(126, attributes);

	etimer_set(&et_tdma, slot_length);
	etimer_set(&et_round, round_length);

	while (true)
	{
		PROCESS_WAIT_EVENT();

		if (etimer_expired(&et_tdma))
		{
			etimer_set(&et_tdma, slot_length);

			// If this is our assigned slot, then we might broadcast
			if (current_slot == assigned_slot)
			{
				// Check that there is something to broadcast
				if (!linked_list_is_empty(&packet_queue))
				{
					// Get the first entry in the queue
					queue_entry_t * entry = (queue_entry_t *)linked_list_peek(&packet_queue);

					bcast(&bc, entry->data, entry->length, entry->type);

					// Remove the entry we just sent
					linked_list_pop(&packet_queue);
				}
			}

			// Move to next slot
			current_slot += 1;
			current_slot %= slot_count;
		}

		if (etimer_expired(&et_round))
		{
			etimer_set(&et_round, round_length);

			printf("Broadcasting our slot %u\n", assigned_slot);

			bcast_beacon(&bc, false);
		}
	}

exit:
	broadcast_close(&bc);
	map_free(&neighbour_details);
	linked_list_free(&packet_queue);
	unique_array_free(&one_hop_neighbours);
	PROCESS_END();
}
