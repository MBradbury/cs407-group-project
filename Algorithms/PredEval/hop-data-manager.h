#ifndef CS407_HOP_DATA_MANAGER_H
#define CS407_HOP_DATA_MANAGER_H

#include "containers/array-list.h"
#include "containers/map.h"

#include "net/rime.h"

#include "predicate-manager.h"

#include <string.h>

typedef struct hop_data
{
	array_list_t maps;
	unsigned int max_size;

} hop_data_t;


bool hop_manager_init(hop_data_t * hop_data);
bool hop_manager_free(hop_data_t * hop_data);

bool hop_manager_record(hop_data_t * hop_data, uint8_t hops, void const * data, size_t data_length);
bool hop_manager_remove(hop_data_t * hop_data, uint8_t hops, rimeaddr_t const * from);

bool hop_manager_reset(hop_data_t * hop_data);

map_t * hop_manager_get(hop_data_t * hop_data, uint8_t hops);

unsigned int hop_manager_max_size(hop_data_t * hop_data);

unsigned int hop_manager_length(hop_data_t * hop_data, var_elem_t const * variable);


#endif /*CS407_HOP_DATA_MANAGER_H*/