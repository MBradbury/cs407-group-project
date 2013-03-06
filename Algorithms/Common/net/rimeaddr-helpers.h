#ifndef CS407_RIMEADDR_HELPERS_H
#define CS407_RIMEADDR_HELPERS_H

#include "net/rime.h"

#include <stdbool.h>

typedef struct rimeaddr_pair
{
	rimeaddr_t first, second;
} rimeaddr_pair_t;

bool rimeaddr_equality(void const * left, void const * right);

void * rimeaddr_clone(void const * addr);

bool rimeaddr_pair_equality(void const * left, void const * right);

#endif /*CS407_RIMEADDR_HELPERS_H*/

