#include "predicate-manager.h"

#include "contiki.h"
#include "dev/leds.h"

#include "debug-helper.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Struct recieved from base station that contains a predicate to be evaluated by this node.
typedef struct 
{
	uint8_t predicate_id;
	uint8_t bytecode_length; // length of the bytecode_instructions, located after the struct
	uint8_t num_of_bytecode_var; // number of variables after the struct
} eval_pred_req_t;


// The custom headers we use
static const struct packetbuf_attrlist trickle_attributes[] = {
	{ PACKETBUF_ADDR_ERECEIVER, PACKETBUF_ADDRSIZE },
	TRICKLE_ATTRIBUTES
    PACKETBUF_ATTR_LAST
};


static void predicate_detail_entry_cleanup(void * item)
{
	predicate_detail_entry_t * entry = (predicate_detail_entry_t *)item;

	free(entry->variables_details);
	free(entry->bytecode);
	free(entry);
}

static bool predicate_id_equal(void const * left, void const * right)
{
	if (left == NULL || right == NULL)
		return false;

	predicate_detail_entry_t const * l = (predicate_detail_entry_t const *)left;
	predicate_detail_entry_t const * r = (predicate_detail_entry_t const *)right;

	return l->id == r->id;
}



static void trickle_recv(struct trickle_conn * tc)
{
	predicate_manager_conn_t * conn = (predicate_manager_conn_t *)tc;

	eval_pred_req_t const * msg = (eval_pred_req_t *)packetbuf_dataptr();

	// Get eventual destination from header
	rimeaddr_t const * target = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

	//printf("Rcv packet length %d\n", packetbuf_datalen());

	if (msg->bytecode_length == 0)
	{
		// There is no bytecode, so interpret this as a request to stop
		// evaluating this predicate
		map_remove(&conn->predicates, &msg->predicate_id);

		printf("Predicate Manager: Removed predicate with id %d\n", msg->predicate_id);
	}
	else
	{
		// Add or update entry
		predicate_detail_entry_t * stored = (predicate_detail_entry_t *)map_get(&conn->predicates, &msg->predicate_id);

		if (stored)
		{
			printf("Predicate Manager: Updating predicate with id %d.\n", msg->predicate_id);

			// Re-allocate data structures if needed

			if (msg->bytecode_length != stored->bytecode_length)
			{
				free(stored->bytecode);
				stored->bytecode = malloc(sizeof(ubyte) * msg->bytecode_length);
			}

			if (msg->num_of_bytecode_var != stored->variables_details_length)
			{
				free(stored->variables_details);
				stored->variables_details = malloc(sizeof(var_elem_t) * msg->num_of_bytecode_var);
			}
		}
		else
		{
			printf("Predicate Manager: Creating predicate with id %d.\n", msg->predicate_id);

			// Allocate memory for the data
			stored = malloc(sizeof(predicate_detail_entry_t));

			stored->id = msg->predicate_id; //set the key
			stored->bytecode = malloc(sizeof(ubyte) * msg->bytecode_length);
			stored->variables_details = malloc(sizeof(var_elem_t) * msg->num_of_bytecode_var);

			// Put data in the map
			map_put(&conn->predicates, stored);
		}

		// Update the target of this predicate
		rimeaddr_copy(&stored->target, target);

		// Pointer for bytecode variables
		var_elem_t const * variables = (var_elem_t const *)(msg + 1);

		// Create a pointer to the bytecode instructions stored in the message.
		ubyte const * bytecode_instructions = (ubyte const *)(variables + msg->num_of_bytecode_var);

		// Update data
		stored->bytecode_length = msg->bytecode_length;
		stored->variables_details_length = msg->num_of_bytecode_var;

		memcpy(stored->bytecode, bytecode_instructions, sizeof(ubyte) * stored->bytecode_length);
		memcpy(stored->variables_details, variables, sizeof(var_elem_t) * stored->variables_details_length);
	}

	leds_off(LEDS_RED);

	// Set the red led on to indicate that we are evaluating a predicate
	map_elem_t elem;
	for (elem = map_first(&conn->predicates); map_continue(&conn->predicates, elem); elem = map_next(elem))
	{
		predicate_detail_entry_t const * pe = (predicate_detail_entry_t const *)map_data(&conn->predicates, elem);

		// Set the led to be red if this node will evaluate a predicate
		if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) || rimeaddr_cmp(&pe->target, &rimeaddr_null))
		{
			leds_on(LEDS_RED);
			break;
		}
	}

	// Call the updated callback
	conn->updated(conn);
}

static const struct trickle_callbacks tc_callbacks = { &trickle_recv };


bool predicate_manager_open(
	predicate_manager_conn_t * conn, uint16_t ch,
	clock_time_t trickle_interval, predicate_manager_updated_fn updated)
{
	if (conn != NULL && updated != NULL)
	{
		conn->updated = updated;

		map_init(&conn->predicates, &predicate_id_equal, &predicate_detail_entry_cleanup);

		trickle_open(&conn->tc, trickle_interval, ch, &tc_callbacks);
		channel_set_attributes(ch, trickle_attributes);

		return true;
	}

	return false;
}

void predicate_manager_close(predicate_manager_conn_t * conn)
{
	if (conn != NULL)
	{
		trickle_close(&conn->tc);

		map_clear(&conn->predicates);
	}
}

bool predicate_manager_create(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination,
	ubyte const * bytecode, uint8_t bytecode_length,
	var_elem_t const * var_details, uint8_t var_details_length)
{
	if (destination == NULL || bytecode == NULL || bytecode_length == 0 || var_details == NULL || var_details_length == 0)
		return false;

	// Send the request message
	unsigned int packet_size = sizeof(eval_pred_req_t) + (sizeof(ubyte) * bytecode_length) + (sizeof(var_elem_t) * var_details_length);

	packetbuf_clear();
	packetbuf_set_datalen(packet_size);
	debug_packet_size(packet_size);
	eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();
	memset(msg, 0, packet_size);

	// Set eventual destination in header
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, destination);

	msg->predicate_id = id;
	msg->bytecode_length = bytecode_length;
	msg->num_of_bytecode_var = var_details_length;

	var_elem_t * msg_vars = (var_elem_t *)(msg + 1);

	memcpy(msg_vars, var_details, sizeof(var_elem_t) * var_details_length);

	ubyte * msg_bytecode = (ubyte *)(msg_vars + var_details_length);

	// Debug check to make sure that we have done sane things!
	if ((void *)(msg_bytecode + bytecode_length) - (void *)msg != packet_size)
	{
		printf("Failed to copy data correctly got=%ld expected=%u!\n",
			(void *)(msg_bytecode + bytecode_length) - (void *)msg,
			packet_size);
	}

	memcpy(msg_bytecode, bytecode, sizeof(ubyte) * bytecode_length);

	printf("Predicate Manager: Sent packet length %d\n", packet_size);

	trickle_send(&conn->tc);

	return true;
}

bool predicate_manager_cancel(predicate_manager_conn_t * conn, uint8_t id, rimeaddr_t const * destination)
{
	if (conn == NULL || destination == NULL)
		return false;

	unsigned int packet_size = sizeof(eval_pred_req_t);

	packetbuf_clear();
	packetbuf_set_datalen(packet_size);
	debug_packet_size(packet_size);
	eval_pred_req_t * msg = (eval_pred_req_t *)packetbuf_dataptr();
	memset(msg, 0, packet_size);

	// Set eventual destination in header
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, destination);

	msg->predicate_id = id;

	// Setting bytecode length to 0 indicates that this predicate should be removed
	msg->bytecode_length = 0;
	msg->num_of_bytecode_var = 0;

	trickle_send(&conn->tc);

	return true;
}

map_t const * predicate_manager_get_map(predicate_manager_conn_t * conn)
{
	return conn == NULL ? NULL : &conn->predicates;
}
