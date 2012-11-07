#ifndef PREDICATE_CHECKER_H
#define PREDICATE_CHECKER_H

#include <stdbool.h>

#include "net/netstack.h"
#include "net/rime.h"

typedef bool (*predicate_checker_t)(void const *);
typedef void (*predicate_failure_message_t)(void const *);

/** Check a local predicate */
bool check_predicate(
	predicate_checker_t predicate,
	predicate_failure_message_t message,
	void const * state);


typedef struct
{
	double temperature;
	double humidity;
} data_t;

typedef bool (*neighbour_predicate_checker_t)(data_t const *, rimeaddr_t const *);
typedef void (*neighbour_predicate_failure_message_t)(data_t const *, rimeaddr_t const *);

/** Start a process that begins checking a predicate
 *  with respect to the one hop neighbourhood. */
bool check_1_hop_information(
	neighbour_predicate_checker_t predicate,
	neighbour_predicate_failure_message_t message);


#endif /*PREDICATE_CHECKER_H*/

