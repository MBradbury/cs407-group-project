#include "predicate-checker.h"

bool check_predicate(
	predicate_checker_t predicate,
	predicate_failure_message_t message,
	void const * state)
{
	bool result = (*predicate)(state);

	if (!result)
	{
		(*message)(state);
	}

	return result;
}


