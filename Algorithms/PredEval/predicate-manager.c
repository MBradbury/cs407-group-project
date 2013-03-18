#include "predicate-manager.h"

#include "contiki.h"
#include "dev/leds.h"
#include "dev/serial-line.h"

#include "debug-helper.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#define UINT8_MIN 0

PROCESS(predicate_input_process, "PredManager Read Input");

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

bool predicate_manager_start_serial_input(predicate_manager_conn_t * conn)
{
	if (conn == NULL)
	{
		return false;
	}

	process_start(&predicate_input_process, (void *)conn);

	return true;
}

void predicate_manager_close(predicate_manager_conn_t * conn)
{
	if (conn != NULL)
	{
		// Shut down the serial input process if it is running
		if (process_is_running(&predicate_input_process))
		{
			process_exit(&predicate_input_process);
		}

		trickle_close(&conn->tc);

		map_free(&conn->predicates);
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


uint8_t predicate_manager_max_hop(predicate_detail_entry_t const * pe)
{
	if (pe == NULL)
	{
		return 0;
	}

	uint8_t max_hops = 0;

	unsigned int i;
	for (i = 0; i < pe->variables_details_length; ++i)
	{
		if (pe->variables_details[i].hops > max_hops)
		{
			max_hops = pe->variables_details[i].hops;
		}

		//printf("variables added: %d %d\n",varmap_cleariables[i].hops,variables[i].var_id);
	}

	return max_hops;
}



PROCESS_THREAD(predicate_input_process, ev, data)
{
	static predicate_manager_conn_t * conn;
	static predicate_detail_entry_t current;
	static int state;

	PROCESS_BEGIN();

	conn = (predicate_manager_conn_t *)data;

	memset(&current, 0, sizeof(predicate_detail_entry_t));
	state = 0;

	while (true)
	{
		// Let others do work until we have a line to process
		PROCESS_YIELD_UNTIL(ev == serial_line_event_message);

		char const * line = (char const *)data;
		unsigned int length = strlen(line);

		printf("Predicate Manager: received line: `%s' of length %u\n", line, length);

		switch (state)
		{
		// Initial state looking for start line
		case 0:
			{
				if (strcmp(line, "["))
				{
					state = 1;
				}
			} break;

		// Read in the predicate id
		case 1:
			{
				int value = atoi(line);

				if (value >= UINT8_MIN && value <= UINT8_MAX)
				{
					current.id = (uint8_t)value;
					state = 2;
				}
				else
				{
					state = 99;
					continue;
				}
			} break;

		// Read in the predicate target
		case 2:
			{
				char buffer[4];

				char const * position = strchr(line, '.');
				unsigned int first_length = position - line;

				memcpy(buffer, line, first_length);
				buffer[first_length] = '\0';

				// Before dot
				current.target.u8[0] = atoi(buffer);

				// After dot
				current.target.u8[1] = atoi(position + 1);

				state = 3;

			} break;

		// Read in the variable ids or bytecode
		case 3:
			{
				if (line[0] == 'b')
				{
					unsigned int bytecode_count = (length - 1) / 2;
					ubyte * starting = NULL;

					if (current.bytecode == NULL)
					{
						current.bytecode = malloc(sizeof(ubyte) * ((length - 1) / 2));

						starting = current.bytecode;
					}
					else
					{
						ubyte * new = malloc(sizeof(ubyte) * (bytecode_count + current.bytecode_length));
						memcpy(new, current.bytecode, sizeof(ubyte) * current.bytecode_length);
						free(current.bytecode);
						current.bytecode = new;

						starting = current.bytecode + current.bytecode_length;
					}

					char const * current_pair = line + 1;

					unsigned int i = 0;
					for (i = 0; i != bytecode_count; ++i)
					{
						char buffer[3];
						memcpy(buffer, current_pair, sizeof(char) * 2);
						buffer[2] = '\0';

						starting[i] = (ubyte) strtol(buffer, NULL, 16);

						current_pair += 2;
					}

					// Record the newly added bytecode
					current.bytecode_length += bytecode_count;
				}
				else if (line[0] == 'v')
				{
					var_elem_t * to_store_at = NULL;
					if (current.variables_details == NULL)
					{
						current.variables_details = malloc(sizeof(var_elem_t));
						to_store_at = current.variables_details;
					}
					else
					{
						var_elem_t * new = malloc(sizeof(var_elem_t) * (1 + current.variables_details_length));
						memcpy(new, current.variables_details, sizeof(var_elem_t) * current.variables_details_length);
						free(current.variables_details);
						current.variables_details = new;

						to_store_at = current.variables_details + current.variables_details_length;
					}

					char const * start = line + 1;

					char buffer[4];

					char const * position = strchr(start, '.');
					unsigned int first_length = position - start;

					memcpy(buffer, line, first_length);
					buffer[first_length] = '\0';

					// Before dot
					to_store_at->hops = atoi(buffer);

					// After dot
					to_store_at->var_id = atoi(position + 1);


					current.variables_details_length += 1;

				}
				else if (strcmp(line, "]"))
				{
					if (current.bytecode_length == 0)
					{
						state = 99;
						continue;
					}

					predicate_manager_create(conn,
						current.id, &current.target,
						current.bytecode, current.bytecode_length,
						current.variables_details, current.variables_details_length);

					free(current.bytecode);
					free(current.variables_details);
					memset(&current, 0, sizeof(predicate_detail_entry_t));

					state = 0;
				}
				else
				{
					state = 99;
					continue;
				}

			} break;

		// Error state
		case 99:
			{
				free(current.variables_details);
				free(current.bytecode);
				memset(&current, 0, sizeof(predicate_detail_entry_t));
				printf("Predicate Manager: Error occured in parsing input\n");
			} break;

		default:
			printf("Predicate Manager: Not sure what to do with state %d and line %s\n", state, line);
			break;
		}

	}

	PROCESS_END();
}
