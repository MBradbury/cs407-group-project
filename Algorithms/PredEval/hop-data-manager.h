#ifndef CS407_HOP_DATA_MANAGER_H
#define CS407_HOP_DATA_MANAGER_H

#include "containers/array-list.h"
#include "containers/map.h"

#include "net/rime.h"

#include "predicate-manager.h"

#include <string.h>

// This library is a component of predicate evaluation
// it is used to manage the data structures that contain
// node information a certain number of hops away.

typedef struct hop_data
{
	array_list_t maps; // List of maps 
	unsigned int max_size;
} hop_data_t;

bool hop_manager_init(hop_data_t * hop_data);
void hop_manager_free(hop_data_t * hop_data);

bool hop_manager_record(hop_data_t * hop_data,
	uint8_t hops, void const * data, size_t data_length);
void hop_manager_remove(hop_data_t * hop_data,
	uint8_t hops, rimeaddr_t const * from);

void hop_manager_reset(hop_data_t * hop_data);

// Gets a map of data on nodes that a a certain number of hops away
map_t * hop_manager_get(hop_data_t * hop_data, uint8_t hops);

unsigned int hop_manager_length(hop_data_t * hop_data, var_elem_t const * variable);

#ifdef CONTAINERS_CHECKED
#	define hop_manager_max_size(hop_data) \
		(hop_data != NULL ? hop_data->max_size : 0)
#else
#	define hop_manager_max_size(hop_data) \
		((hop_data)->max_size)
#endif

#endif /*CS407_HOP_DATA_MANAGER_H*/