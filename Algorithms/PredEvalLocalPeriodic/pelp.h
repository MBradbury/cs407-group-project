#ifndef CS407_PELP_H
#define CS407_PELP_H

#include <stdbool.h>
#include <stdint.h>

#include "predlang.h"
#include "nhopreq.h"
#include "predicate-manager.h"
#include "hop-data-manager.h"

struct pelp_conn;

typedef void (* pelp_predicate_failed_fn)(struct pelp_conn * conn, rimeaddr_t const * from, uint8_t hops);

typedef struct pelp_conn
{
	nhopreq_conn_t nhr;
	predicate_manager_conn_t predconn;
	hop_data_t hop_data;

	rimeaddr_t const * sink;

	pelp_predicate_failed_fn predicate_failed;
	node_data_fn data_fn;
	size_t data_size;

	uint8_t max_comm_hops;

	uint8_t functions_count;
	function_details_t const * function_details;

	clock_time_t predicate_period;

} pelp_conn_t;

bool pelp_start(pelp_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pelp_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count,
	clock_time_t predicate_period);

void pelp_stop(pelp_conn_t * conn);


#endif /*CS407_PELP_H*/
