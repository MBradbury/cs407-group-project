#ifndef CS407_PREDICATE_MANAGER_H
#define CS407_PREDICATE_MANAGER_H

#include "containers/map.h"

#include "trickle.h"
#include "mesh.h"

#include "predlang.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef struct
{
	uint8_t id;
	uint8_t return_type;
	void const * (* fn)(void const * ptr);
} function_details_t;

typedef struct
{
	uint8_t predicate_id;
	uint8_t num_hops_positions;
	uint8_t data_length;
} failure_response_t;

typedef struct
{
	uint8_t hops;
	uint8_t var_id;
	uint8_t length;
} hops_position_t;


// Struct for the list of bytecode_variables. It contains the variable_id and hop count.
typedef struct
{
	uint8_t hops;
	uint8_t var_id;
} var_elem_t;

struct hop_data;

typedef struct
{
	uint8_t id; // Keep id as the first variable in the struct
	uint8_t variables_details_length;
	uint8_t bytecode_length;

	rimeaddr_t target;

	var_elem_t * variables_details;
	ubyte * bytecode;

} predicate_detail_entry_t;

struct predicate_manager_conn;

typedef struct
{
	void (* updated)(struct predicate_manager_conn * conn);

	void (* recv_response)(struct predicate_manager_conn * conn, rimeaddr_t const * from, uint8_t hops);
} predicate_manager_callbacks_t;

typedef struct predicate_manager_conn
{
	struct trickle_conn tc;
	struct mesh_conn mc;

	map_t predicates;

	rimeaddr_t basestation;

	predicate_manager_callbacks_t const * callbacks;

} predicate_manager_conn_t;

bool predicate_manager_open(
	predicate_manager_conn_t * conn, uint16_t ch1, uint16_t ch2, rimeaddr_t const * basestation,
	clock_time_t trickle_interval, predicate_manager_callbacks_t const * callbacks);

void predicate_manager_start_serial_input(predicate_manager_conn_t * conn);

void predicate_manager_close(predicate_manager_conn_t * conn);

bool predicate_manager_create(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination,
	ubyte const * bytecode, uint8_t bytecode_length,
	var_elem_t const * var_details, uint8_t var_details_length);

bool predicate_manager_cancel(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination);


bool predicate_manager_send_response(predicate_manager_conn_t * conn, struct hop_data * hop_data,
	predicate_detail_entry_t const * pe, void * data, size_t data_size, size_t data_length);


#define predicate_manager_get_map(conn) ((conn) == NULL ? NULL : &(conn)->predicates)

uint8_t predicate_manager_max_hop(predicate_detail_entry_t const * pe);


bool evaluate_predicate(predicate_manager_conn_t * conn,
	node_data_fn data_fn, size_t data_size,
	function_details_t const * function_details, size_t functions_count,
	struct hop_data * hop_data,
	void const * all_neighbour_data, unsigned int nd_length,
	predicate_detail_entry_t const * pe);

#endif /*CS407_PREDICATE_MANAGER_H*/
