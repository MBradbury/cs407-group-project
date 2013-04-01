#ifndef CS407_PELE_H
#define CS407_PELE_H

#include <stdbool.h>
#include <stdint.h>

#include "predlang.h"
#include "net/eventupdate.h"
#include "predicate-manager.h"
#include "hop-data-manager.h"

struct pele_conn;

typedef bool (* pele_data_differs_fn)(void const * data1, void const * data2);
typedef void (* pele_predicate_failed_fn)(struct pele_conn * conn, rimeaddr_t const * from, uint8_t hops);

typedef struct pele_conn
{
	event_update_conn_t euc;
	predicate_manager_conn_t predconn;
	hop_data_t hop_data;

	rimeaddr_t const * sink;

	pele_predicate_failed_fn predicate_failed;
	node_data_fn data_fn;
	pele_data_differs_fn differs_fn;
	size_t data_size;

	uint8_t max_comm_hops;

	uint8_t functions_count;
	function_details_t const * function_details;

	clock_time_t predicate_period;

} pele_conn_t;

bool pele_start(pele_conn_t * conn,
	rimeaddr_t const * sink, node_data_fn data_fn, size_t data_size,
	pele_data_differs_fn differs_fn, pele_predicate_failed_fn predicate_failed,
	function_details_t const * function_details, uint8_t functions_count,
	clock_time_t predicate_period);

void pele_stop(pele_conn_t * conn);


#endif /*CS407_PELE_H*/
