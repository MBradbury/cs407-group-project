#ifndef CS407_PEGP_H
#define CS407_PEGP_H

#include <stdbool.h>
#include <stdint.h>

#include "predlang.h"
#include "predicate-manager.h"
#include "hop-data-manager.h"
#include "net/tree-aggregator.h"
#include "neighbour-aggregate.h"

#include "containers/unique-array.h"
#include "containers/map.h"

struct pegp_conn;

typedef void (* pegp_predicate_failed_fn)(struct pegp_conn * conn, rimeaddr_t const * from, uint8_t hops);

typedef struct pegp_conn
{
	tree_agg_conn_t aggconn;
	neighbour_agg_conn_t nconn;
	predicate_manager_conn_t predconn;
	hop_data_t hop_data;

	rimeaddr_t const * sink;

	pegp_predicate_failed_fn predicate_failed;
	node_data_fn data_fn;
	size_t data_size;

	//uint8_t max_comm_hops;

	uint8_t functions_count;
	function_details_t const * function_details;

	// Map containing rimeaddr_pair_t
	unique_array_t neighbour_info;

	// Map containing node_data_t
	map_t received_data;

	// Used for simulating evaluating a predicate on a node
	rimeaddr_t pred_simulated_node;

	unsigned int pred_round_count;

	struct ctimer ct_startup;

	clock_time_t predicate_period;

} pegp_conn_t;

bool pegp_start(pegp_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pegp_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count,
	clock_time_t predicate_period);

void pegp_stop(pegp_conn_t * conn);


#endif /*CS407_PEGP_H*/
