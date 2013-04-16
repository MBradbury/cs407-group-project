#include "predicate-manager.h"

#include "contiki.h"
#include "dev/leds.h"
#include "dev/serial-line.h"

#include "debug-helper.h"
#include "hop-data-manager.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#ifdef PREDICATE_MANAGER_DEBUG
#	define PMDPRINTF(...) printf(__VA_ARGS__)
#else
#	define PMDPRINTF(...)
#endif

// Helper macro
#define UINT8_MIN 0

// Amount of time to wait for a mesh response
static const clock_time_t MESH_WAIT_PERIOD = 120 * CLOCK_SECOND;

// The process that handles serial input
PROCESS(predicate_input_process, "PredManager Read Input");

// Repurpose packetbuf attributes
#define PACKETBUF_ATTR_PREDICATE_ID PACKETBUF_ATTR_HOPS
#define PACKETBUF_ATTR_BYTECODE_LEN PACKETBUF_ATTR_ERELIABLE
#define PACKETBUF_ATTR_VAR_LEN PACKETBUF_ATTR_EPACKET_TYPE

// The custom headers we use
static const struct packetbuf_attrlist trickle_attributes[] = {
	{ PACKETBUF_ATTR_PREDICATE_ID, PACKETBUF_ATTR_BYTE },
	{ PACKETBUF_ATTR_BYTECODE_LEN, PACKETBUF_ATTR_BYTE },
	{ PACKETBUF_ATTR_VAR_LEN, PACKETBUF_ATTR_BYTE },

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

	uint8_t const * l = (uint8_t const *)left;
	uint8_t const * r = (uint8_t const *)right;

	return *l == *r;
}


static inline predicate_manager_conn_t * conncvt_trickle(struct trickle_conn * conn)
{
	return (predicate_manager_conn_t *)conn;
}

static inline predicate_manager_conn_t * conncvt_mesh(struct mesh_conn * conn)
{
	return (predicate_manager_conn_t *)
		(((char *)conn) - sizeof(struct trickle_conn));
}


static void trickle_recv(struct trickle_conn * tc)
{
	predicate_manager_conn_t * conn = conncvt_trickle(tc);

	void const * msg = packetbuf_dataptr();

	uint8_t predicate_id = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_PREDICATE_ID);
	uint8_t bytecode_length = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_BYTECODE_LEN);
	uint8_t variables_length = (uint8_t)packetbuf_attr(PACKETBUF_ATTR_VAR_LEN);

	// Get eventual destination from header
	rimeaddr_t const * target = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

	if (bytecode_length == 0)
	{
		// There is no bytecode, so interpret this as a request to stop evaluating this predicate
		map_remove(&conn->predicates, &predicate_id);

		printf("PredMan: Remove %u\n", predicate_id);
	}
	else
	{
		// Add or update entry
		predicate_detail_entry_t * stored =
			(predicate_detail_entry_t *)map_get(&conn->predicates, &predicate_id);

		if (stored != NULL)
		{
			printf("PredMan: Update %u\n", predicate_id);

			// Re-allocate data structures if needed

			if (bytecode_length != stored->bytecode_length)
			{
				free(stored->bytecode);
				stored->bytecode = malloc(sizeof(ubyte) * bytecode_length);
			}

			if (variables_length != stored->variables_details_length)
			{
				free(stored->variables_details);
				stored->variables_details = malloc(sizeof(var_elem_t) * variables_length);
			}
		}
		else
		{
			printf("PredMan: Create %u\n", predicate_id);

			// Allocate memory for the data
			stored = malloc(sizeof(predicate_detail_entry_t));

			stored->id = predicate_id; // Set the key
			stored->bytecode = malloc(sizeof(ubyte) * bytecode_length);
			stored->variables_details = malloc(sizeof(var_elem_t) * variables_length);

			// Put data in the map
			map_put(&conn->predicates, stored);
		}

		// Update the target of this predicate
		rimeaddr_copy(&stored->target, target);

		// Pointer for bytecode variables
		var_elem_t const * variables = (var_elem_t const *)msg;

		// Create a pointer to the bytecode instructions stored in the message.
		ubyte const * bytecode_instructions = (ubyte const *)(variables + variables_length);

		// Update data
		stored->bytecode_length = bytecode_length;
		stored->variables_details_length = variables_length;

		memcpy(stored->bytecode, bytecode_instructions,
			sizeof(ubyte) * stored->bytecode_length);
		memcpy(stored->variables_details, variables,
			sizeof(var_elem_t) * stored->variables_details_length);
	}

	leds_off(LEDS_RED);

	// Set the red led on to indicate that we are evaluating a predicate
	map_elem_t elem;
	for (elem = map_first(&conn->predicates);
		 map_continue(&conn->predicates, elem);
		 elem = map_next(elem))
	{
		predicate_detail_entry_t const * pe =
			(predicate_detail_entry_t const *)map_data(&conn->predicates, elem);

		// Set the led to be red if this node will evaluate a predicate
		if (rimeaddr_cmp(&pe->target, &rimeaddr_node_addr) ||
			rimeaddr_cmp(&pe->target, &rimeaddr_null))
		{
			leds_on(LEDS_RED);
			break;
		}
	}

	// Call the updated callback
	if (conn->callbacks->updated != NULL)
	{
		conn->callbacks->updated(conn);
	}
}

static const struct trickle_callbacks tc_callbacks = { &trickle_recv };


// Used to handle receiving predicate failure messages
static void mesh_rcv(struct mesh_conn * c, rimeaddr_t const * from, uint8_t hops)
{
	predicate_manager_conn_t * conn = conncvt_mesh(c);

	if (conn->callbacks->recv_response != NULL)
	{
		conn->callbacks->recv_response(conn, from, hops);
	}
}

static void mesh_timeout(struct mesh_conn * c)
{
	predicate_manager_conn_t * conn = conncvt_mesh(c);

	printf("PredMan: PF reply timedout\n");
}

static const struct mesh_callbacks mc_callbacks = { &mesh_rcv, NULL, &mesh_timeout };


bool predicate_manager_open(
	predicate_manager_conn_t * conn, uint16_t ch1, uint16_t ch2, rimeaddr_t const * basestation,
	clock_time_t trickle_interval, predicate_manager_callbacks_t const * callbacks)
{
	if (conn != NULL && callbacks != NULL && basestation != NULL)
	{
		conn->callbacks = callbacks;

		rimeaddr_copy(&conn->basestation, basestation);

		map_init(&conn->predicates, &predicate_id_equal, &predicate_detail_entry_cleanup);

		trickle_open(&conn->tc, trickle_interval, ch1, &tc_callbacks);
		channel_set_attributes(ch1, trickle_attributes);

		mesh_open(&conn->mc, ch2, &mc_callbacks, MESH_WAIT_PERIOD);

		return true;
	}

	return false;
}

void predicate_manager_start_serial_input(predicate_manager_conn_t * conn)
{
	if (conn != NULL)
	{
		process_start(&predicate_input_process, (void *)conn);
	}
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

		mesh_close(&conn->mc);

		map_free(&conn->predicates);
	}
}

bool predicate_manager_create(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination,
	ubyte const * bytecode, uint8_t bytecode_length,
	var_elem_t const * var_details, uint8_t var_details_length)
{
	if (destination == NULL || bytecode == NULL || bytecode_length == 0 ||
		var_details == NULL || var_details_length == 0)
		return false;

	// Send the request message
	const unsigned int packet_size =
		(sizeof(ubyte) * bytecode_length) +
		(sizeof(var_elem_t) * var_details_length);

	if (packet_size > PACKETBUF_SIZE)
	{
		printf("PredMan: Packet too long\n");
		return false;
	}

	packetbuf_clear();
	packetbuf_set_datalen(packet_size);
	void * msg = packetbuf_dataptr();

	// Set eventual destination in header
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, destination);

	packetbuf_set_attr(PACKETBUF_ATTR_PREDICATE_ID, id);
	packetbuf_set_attr(PACKETBUF_ATTR_BYTECODE_LEN, bytecode_length);
	packetbuf_set_attr(PACKETBUF_ATTR_VAR_LEN, var_details_length);

	var_elem_t * msg_vars = (var_elem_t *)msg;

	memcpy(msg_vars, var_details, sizeof(var_elem_t) * var_details_length);

	ubyte * msg_bytecode = (ubyte *)(msg_vars + var_details_length);

#ifdef PREDICATE_MANAGER_DEBUG
	// Debug check to make sure that we have done sane things!
	if ((void *)(msg_bytecode + bytecode_length) - (void *)msg != packet_size)
	{
		printf("PredMan: Failed copy got=%ld expected=%u!\n",
			(void *)(msg_bytecode + bytecode_length) - (void *)msg,
			packet_size);
	}
#endif

	memcpy(msg_bytecode, bytecode, sizeof(ubyte) * bytecode_length);

	PMDPRINTF("PredMan: Sent %d\n", packet_size);

	// We need to receive the predicate so we know of it
	trickle_recv(&conn->tc);

	trickle_send(&conn->tc);

	return true;
}

bool predicate_manager_cancel(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination)
{
	if (conn == NULL || destination == NULL)
		return false;

	packetbuf_clear();
	packetbuf_set_datalen(1);

	// Set eventual destination in header
	packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, destination);

	packetbuf_set_attr(PACKETBUF_ATTR_PREDICATE_ID, id);

	// Setting bytecode length to 0 indicates that this predicate should be removed
	packetbuf_set_attr(PACKETBUF_ATTR_BYTECODE_LEN, 0);
	packetbuf_set_attr(PACKETBUF_ATTR_VAR_LEN, 0);

	trickle_send(&conn->tc);

	return true;
}

bool predicate_manager_send_response(predicate_manager_conn_t * conn, hop_data_t * hop_data,
	predicate_detail_entry_t const * pe, void const * data, size_t data_size, size_t data_length,
	node_data_fn node_data, bool result)
{
	if (conn == NULL || pe == NULL)
	{
		return false;
	}

	const unsigned int packet_length =
		sizeof(failure_response_t) +
		sizeof(hops_position_t) * pe->variables_details_length +
		data_size * (data_length + 1);

	if (packet_length > PACKETBUF_SIZE)
	{
		printf("PredMan: Pred reply too long %u > %d\n", packet_length, PACKETBUF_SIZE);

		return false;
	}

	// TODO: Switch to using ruldolph1 or our own multipacket
	// otherwise this cannot handle packets larger than 128 bytes

	packetbuf_clear();
	packetbuf_set_datalen(packet_length);
	failure_response_t * msg = (failure_response_t *)packetbuf_dataptr();

	msg->predicate_id = pe->id;
	msg->num_hops_positions = pe->variables_details_length;
	msg->data_length = data_length + 1;
	msg->result = result ? 1 : 0;

	hops_position_t * hops_details = (hops_position_t *)(msg + 1);

	uint8_t i;
	for (i = 0; i < msg->num_hops_positions; ++i)
	{
		hops_details[i].hops = pe->variables_details[i].hops;
		hops_details[i].var_id = pe->variables_details[i].var_id;
		hops_details[i].length = hop_manager_length(hop_data, &pe->variables_details[i]);
	}

	void * msg_neighbour_data = (void *)(hops_details + pe->variables_details_length);

	// Copy in this node's data
	node_data(msg_neighbour_data);

	msg_neighbour_data = ((char *)msg_neighbour_data) + data_size;

	// Copy in neighbour data
	memcpy(msg_neighbour_data, data, data_size * data_length);

	// Make sure we have a backup of the packet
	// Just incase the receiver function clears it
	char tmpBuffer[PACKETBUF_SIZE];
	memcpy(tmpBuffer, msg, packet_length);

	// Also have the sender receive the message
	// This will cause the details to be printed out for analysis
	mesh_rcv(&conn->mc, &rimeaddr_node_addr, 0);

	// If the target is not the current node send the message
	if (!rimeaddr_cmp(&conn->basestation, &rimeaddr_node_addr) && !result)
	{
		packetbuf_clear();
		packetbuf_set_datalen(packet_length);
		msg = (failure_response_t *)packetbuf_dataptr();

		// Copy in the packet backup
		memcpy(msg, tmpBuffer, packet_length);

		mesh_send(&conn->mc, &conn->basestation);
	}

	return true;
}


uint8_t predicate_manager_max_hop(predicate_detail_entry_t const * pe)
{
	if (pe == NULL)
	{
		return 0;
	}

	uint8_t max_hops = 0;

	uint8_t i;
	for (i = 0; i < pe->variables_details_length; ++i)
	{
		if (pe->variables_details[i].hops > max_hops)
		{
			max_hops = pe->variables_details[i].hops;
		}
	}

	return max_hops;
}


bool evaluate_predicate(predicate_manager_conn_t * conn,
	node_data_fn data_fn, size_t data_size,
	function_details_t const * function_details, size_t functions_count,
	hop_data_t * hop_data,
	void const * all_neighbour_data, unsigned int nd_length,
	predicate_detail_entry_t const * pe)
{
	unsigned int i;

	// Set up the predicate language VM
	init_pred_lang(data_fn, data_size);

	// Register the data functions

	for (i = 0; i < functions_count; ++i)
	{
		function_details_t const * fund = &function_details[i];

		register_function(fund->id, fund->fn, fund->return_type);
	}

	// Bind the variables to the VM
	for (i = 0; i < pe->variables_details_length; ++i)
	{
		// Get the length of this hop's data
		// including all of the closer hop's data length
		unsigned int length = hop_manager_length(hop_data, &pe->variables_details[i]);

		printf("PredMan: Binding vars: id=%d hop=%d len=%d\n",
			pe->variables_details[i].var_id, pe->variables_details[i].hops, length);
		
		bind_input(pe->variables_details[i].var_id, all_neighbour_data, length);
	}

	bool result = evaluate(pe->bytecode, pe->bytecode_length);

	predicate_manager_send_response(conn, hop_data,
		pe, all_neighbour_data, data_size, nd_length, data_fn, result);

	return result;
}

void print_node_data(void const * ptr, function_details_t const * fn_details, size_t fn_count)
{
	size_t i;
	for (i = 0; i != fn_count; ++i)
	{
		printf("%u=", fn_details[i].id);

		void const * data = fn_details[i].fn(ptr);

		switch (fn_details[i].return_type)
		{
		case TYPE_INTEGER:
			{
				nint x = *(nint const *)data;
				printf("%i", x);
			} break;

		case TYPE_FLOATING:
			{
				double f = *(nfloat const *)data;
				printf("%f", f);
			} break;

		default:
			printf("unk");
			break;
		}

		if (i + 1 < fn_count)
		{
			printf(",");
		}
	}
}

// Simple, no error checking to reduce firmware size
#define HEX_CHAR_TO_NUM(ch) (((ch) >= '0' && (ch) <= '9') ? (ch) - '0' : (ch) - 'A')

// From: http://www.techinterview.org/post/526339864/int-atoi-char-pstr
static uint8_t myatoi(char const * str)
{
	uint8_t result = 0;

	while (*str && *str >= '0' && *str <= '9')
	{
		result = (result * 10u) + (*str - '0');
		++str;
	}
	
	return result;
}

PROCESS_THREAD(predicate_input_process, ev, data)
{
	static predicate_manager_conn_t * conn;
	static predicate_detail_entry_t current;
	static unsigned int state; // The state in the automata parser

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	conn = (predicate_manager_conn_t *)data;

	memset(&current, 0, sizeof(current));
	state = 0;

	while (true)
	{
		// Let others do work until we have a line to process
		// This prevents us stealing the CPU while doing no work
		PROCESS_YIELD_UNTIL(ev == serial_line_event_message);

		// Get the data from the line message
		char const * line = (char const *)data;
		const unsigned int length = strlen(line);

		PMDPRINTF("PredMan: line:`%s' (%u) in %d\n", line, length, state);

		switch (state)
		{
		// Initial state looking for start line
		case 0:
			{
				if (length == 1 && line[0] == '[' && line[1] == '\0')
				{
					PMDPRINTF("PredMan: Starting predicate input...\n");
					state = 1;
				}
			} break;

		// Read in the predicate id
		case 1:
			{
				unsigned int value = myatoi(line);

				if (value >= UINT8_MIN && value <= UINT8_MAX)
				{
					current.id = (uint8_t)value;
					state = 2;
				}
				else
				{
					PMDPRINTF("PredMan: going to error handler\n");
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
				current.target.u8[0] = (uint8_t)myatoi(buffer);

				// After dot
				current.target.u8[1] = (uint8_t)myatoi(position + 1);

				state = 3;

			} break;

		// Read in the variable ids or bytecode
		case 3:
			{
				if (line[0] == 'b')
				{
					PMDPRINTF("PredMan: processing bytecode\n");

					unsigned int bytecode_count = (length - 1) / 2;
					
					ubyte * new = malloc(sizeof(ubyte) * (bytecode_count + current.bytecode_length));
					memcpy(new, current.bytecode, sizeof(ubyte) * current.bytecode_length);
					free(current.bytecode);
					current.bytecode = new;

					ubyte * starting = current.bytecode + current.bytecode_length;

					// Start looking at characters after the first b
					char const * current_pair = line + 1;

					unsigned int i = 0;
					for (i = 0; i != bytecode_count; ++i)
					{
						starting[i] = HEX_CHAR_TO_NUM(current_pair[0]) * 16 +
									  HEX_CHAR_TO_NUM(current_pair[1]);

						current_pair += 2;
					}

					// Record the newly added bytecode
					current.bytecode_length += bytecode_count;
				}
				else if (line[0] == 'v')
				{
					PMDPRINTF("PredMan: processing variable details\n");

					var_elem_t * new = malloc(
						sizeof(var_elem_t) * (1 + current.variables_details_length));
					
					memcpy(new, current.variables_details,
						sizeof(var_elem_t) * current.variables_details_length);
					
					free(current.variables_details);
					current.variables_details = new;

					var_elem_t * to_store_at =
						current.variables_details + current.variables_details_length;

					char const * start = line + 1;

					char buffer[4];

					char const * position = strchr(start, '.');
					unsigned int first_length = position - start;

					memcpy(buffer, line, first_length);
					buffer[first_length] = '\0';

					// Before dot
					to_store_at->hops = (uint8_t)myatoi(buffer);

					// After dot
					to_store_at->var_id = (uint8_t)myatoi(position + 1);

					current.variables_details_length += 1;
				}
				else if (length == 1 && line[0] == ']' && line[1] == '\0')
				{
					if (current.bytecode_length == 0)
					{
						predicate_manager_cancel(conn, current.id, &current.target);
					}
					else
					{
						predicate_manager_create(conn,
							current.id, &current.target,
							current.bytecode, current.bytecode_length,
							current.variables_details, current.variables_details_length);
					}

					free(current.bytecode);
					free(current.variables_details);
					memset(&current, 0, sizeof(current));

					state = 0;
				}
				else
				{
					PMDPRINTF("PredMan: going to error handler\n");
					state = 99;
					continue;
				}

			} break;

		// Error state
		case 99:
			{
				free(current.variables_details);
				free(current.bytecode);
				memset(&current, 0, sizeof(current));
				printf("PredMan: Error occured in parsing input\n");
			} break;

		default:
			printf("PredMan: Not sure what to do state=%d, line=%s\n", state, line);
			break;
		}
	}

exit:
	free(current.variables_details);
	free(current.bytecode);
	PROCESS_END();
}
