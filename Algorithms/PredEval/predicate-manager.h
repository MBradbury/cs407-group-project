#ifndef CS407_PREDICATE_MANAGER_H
#define CS407_PREDICATE_MANAGER_H

#include "containers/map.h"

#include "trickle.h"
#include "mesh.h"

#include "predlang.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// This library is a component of predicate evaluation
// it encapsulates storing and evaluating predicates.

// Record of the functions a user has registered
typedef struct
{
	uint8_t id;
	uint8_t return_type;
	void const * (* fn)(void const * ptr);
} function_details_t;

// The message that is sent when a predicate fails
typedef struct
{
	uint8_t predicate_id;
	uint8_t num_hops_positions;
	uint8_t data_length;
	uint8_t result;
} failure_response_t;

// Information about `Neighbour(x)' information and how much data is held
typedef struct
{
	uint8_t hops;
	uint8_t var_id;
	uint8_t length;
} hops_position_t;

// Struct for the list of bytecode_variables
typedef struct
{
	uint8_t hops;
	uint8_t var_id;
} var_elem_t;

struct hop_data;

// Predicate record, predicate received over the network are stored in this
typedef struct
{
	uint8_t id; // Keep id as the first variable in the struct
	uint8_t variables_details_length;
	uint8_t bytecode_length;

	rimeaddr_t target; // rimeaddr_null indicates every node is targeted

	var_elem_t * variables_details;
	ubyte * bytecode;

} predicate_detail_entry_t;

struct predicate_manager_conn;

typedef struct
{
	// Called when a predicate is added, removed or updated
	void (* updated)(struct predicate_manager_conn * conn);

	// Called when a failure message is received.
	// Only needs to be implemented by the base station.
	void (* recv_response)(struct predicate_manager_conn * conn,
		rimeaddr_t const * from, uint8_t hops);
} predicate_manager_callbacks_t;

typedef struct predicate_manager_conn
{
	struct trickle_conn tc;
	struct mesh_conn mc;

	map_t predicates; // Map of id to predicate_detail_entry_t

	rimeaddr_t basestation; // The target of predicate failure messages

	predicate_manager_callbacks_t const * callbacks;

} predicate_manager_conn_t;

bool predicate_manager_open(
	predicate_manager_conn_t * conn, uint16_t ch1, uint16_t ch2, rimeaddr_t const * basestation,
	clock_time_t trickle_interval, predicate_manager_callbacks_t const * callbacks);

// Starts the process that reads serial input containing predicates provided
void predicate_manager_start_serial_input(predicate_manager_conn_t * conn);

void predicate_manager_close(predicate_manager_conn_t * conn);

bool predicate_manager_create(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination,
	ubyte const * bytecode, uint8_t bytecode_length,
	var_elem_t const * var_details, uint8_t var_details_length);

bool predicate_manager_cancel(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination);

#define predicate_manager_get_map(conn) ((conn) == NULL ? NULL : &(conn)->predicates)

uint8_t predicate_manager_max_hop(predicate_detail_entry_t const * pe);

// Actually evaluates the predicate using the provided information
bool evaluate_predicate(predicate_manager_conn_t * conn,
	node_data_fn data_fn, size_t data_size,
	function_details_t const * function_details, size_t functions_count,
	struct hop_data * hop_data,
	void const * all_neighbour_data, unsigned int nd_length,
	predicate_detail_entry_t const * pe);

// A debug helper function
void print_node_data(void const * ptr, function_details_t const * fn_details, size_t fn_count);

#endif /*CS407_PREDICATE_MANAGER_H*/
