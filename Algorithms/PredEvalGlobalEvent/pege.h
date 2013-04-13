#ifndef CS407_PEGE_H
#define CS407_PEGE_H

#include <stdbool.h>
#include <stdint.h>

#include "predlang.h"
#include "predicate-manager.h"
#include "hop-data-manager.h"
#include "net/tree-aggregator.h"
#include "neighbour-aggregate.h"

#include "containers/unique-array.h"
#include "containers/map.h"

struct pege_conn;

typedef bool (* pege_data_differs_fn)(void const * data1, void const * data2);
typedef void (* pege_predicate_failed_fn)(struct pege_conn * conn,
	rimeaddr_t const * from, uint8_t hops);

typedef struct pege_conn
{
	tree_agg_conn_t aggconn;
	neighbour_agg_conn_t nconn;
	predicate_manager_conn_t predconn;

	rimeaddr_t const * sink;

	pege_predicate_failed_fn predicate_failed;
	node_data_fn data_fn;
	pege_data_differs_fn differs_fn;
	size_t data_size;

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

} pege_conn_t;

bool pege_start(pege_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pege_data_differs_fn differs_fn, pege_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count,
	clock_time_t predicate_period);

void pege_stop(pege_conn_t * conn);

#endif /*CS407_PEGE_H*/
