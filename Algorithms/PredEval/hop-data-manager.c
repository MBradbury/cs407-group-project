#include "hop-data-manager.h"

#include "net/rimeaddr-helpers.h"

#include <stddef.h>
#include <stdlib.h>

static void free_hops_data(void * voiddata)
{
	map_t * data = (map_t *)voiddata;
	map_free(data);
	free(data);
}


bool hop_manager_init(hop_data_t * hop_data)
{
	if (hop_data != NULL)
	{
		hop_data->max_size = 0;
		return array_list_init(&hop_data->maps, &free_hops_data);
	}

	return false;
}

bool hop_manager_free(hop_data_t * hop_data)
{
	if (hop_data != NULL)
	{
		hop_data->max_size = 0;
		return array_list_free(&hop_data->maps);
	}
	return false;
}

bool hop_manager_record(hop_data_t * hop_data, uint8_t hops, void const * data, size_t data_length)
{
	if (hop_data == NULL || data == NULL || data_length == 0)
	{
		return false;
	}

	map_t * map = hop_manager_get(hop_data, hops);

	// Check that we have not previously received data from this node before
	void * stored = map_get(map, data);
	
	if (stored == NULL)
	{
		stored = malloc(data_length);

		if (stored == NULL)
		{
			return false;
		}

		memcpy(stored, data, data_length);

		if (!map_put(map, stored))
		{
			free(stored);
			return false;
		}


		hop_data->max_size++;
	}
	else
	{
		// Update the stored data
		memcpy(stored, data, data_length);
	}

	return true;
}

bool hop_manager_remove(hop_data_t * hop_data, uint8_t hops, rimeaddr_t const * from)
{
	if (hop_data == NULL || hops == 0 || from == NULL)
	{
		return false;
	}

	map_t * map = hop_manager_get(hop_data, hops);

	if (map != NULL)
	{
		return map_remove(map, from);
	}

	return true;
}

bool hop_manager_reset(hop_data_t * hop_data)
{
	if (hop_data != NULL)
	{
		hop_data->max_size = 0;
		return array_list_clear(&hop_data->maps);
	}
	return false;
}

map_t * hop_manager_get(hop_data_t * hop_data, uint8_t hop)
{
	if (hop_data == NULL || hop == 0)
	{
		return NULL;
	}

	unsigned int length = array_list_length(&hop_data->maps);

	// Map doesn't exist so create it
	if (length < hop)
	{
		unsigned int to_add;
		for (to_add = hop - length; to_add > 0; --to_add)
		{
			map_t * map = (map_t *)malloc(sizeof(map_t));
			map_init(map, &rimeaddr_equality, &free);

			array_list_append(&hop_data->maps, map);
		}
	}

	return (map_t *)array_list_data(&hop_data->maps, hop - 1);
}

unsigned int hop_manager_max_size(hop_data_t * hop_data)
{
	return hop_data != NULL ? hop_data->max_size : 0;
}

unsigned int hop_manager_length(hop_data_t * hop_data, var_elem_t const * variable)
{
	if (hop_data == NULL || variable == NULL)
	{
		return 0;
	}

	unsigned int length = 0;
	uint8_t j;
	for (j = 1; j <= variable->hops; ++j)
	{
		length += map_length(hop_manager_get(hop_data, j));
	}

	return length;
}
