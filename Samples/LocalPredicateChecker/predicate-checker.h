#ifndef PREDICATE_CHECKER_H
#define PREDICATE_CHECKER_H

#include <stdbool.h>

typedef bool (*predicate_checker_t)(void const *);
typedef void (*predicate_failure_message_t)(void const *);

bool check_predicate(
	predicate_checker_t predicate,
	predicate_failure_message_t message,
	void const * state);


#endif /*PREDICATE_CHECKER_H*/

