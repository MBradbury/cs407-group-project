#ifndef CS407_PREDICATE_MANAGER_H
#define CS407_PREDICATE_MANAGER_H

#include "containers/map.h"

#include "trickle.h"

#include "predlang.h"

#include <stdint.h>
#include <stdbool.h>

// Struct for the list of bytecode_variables. It contains the variable_id and hop count.
typedef struct
{
	uint8_t hops;
	uint8_t var_id;
} var_elem_t;

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

typedef void (* predicate_manager_updated_fn)(struct predicate_manager_conn * conn);

typedef struct predicate_manager_conn
{
	struct trickle_conn tc;

	map_t predicates;

	predicate_manager_updated_fn updated;

} predicate_manager_conn_t;

bool predicate_manager_open(
	predicate_manager_conn_t * conn, uint16_t ch,
	clock_time_t trickle_interval, predicate_manager_updated_fn updated);

void predicate_manager_close(predicate_manager_conn_t * conn);

bool predicate_manager_create(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination,
	ubyte const * bytecode, uint8_t bytecode_length,
	var_elem_t const * var_details, uint8_t var_details_length);

bool predicate_manager_cancel(predicate_manager_conn_t * conn,
	uint8_t id, rimeaddr_t const * destination);

map_t const * predicate_manager_get_map(predicate_manager_conn_t * conn);

uint8_t predicate_manager_max_hop(predicate_detail_entry_t const * pe);


#endif /*CS407_PREDICATE_MANAGER_H*/
