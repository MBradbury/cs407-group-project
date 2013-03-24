#include "containers/map.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

bool map_init(map_t * map, unique_array_equality_t key_equality, array_list_cleanup_t cleanup)
{
	if (map == NULL)
		return false;

	return unique_array_init(map, key_equality, cleanup);
}

bool map_remove(map_t * map, void const * key)
{
	if (map == NULL || key == NULL)
	{
		return false;
	}

	map_elem_t elem;
	for (elem = map_first(map); map_continue(map, elem); elem = map_next(elem))
	{
		void * item = map_data(map, elem);

		if (map->equality(key, item))
		{
			return unique_array_remove(map, elem);
		}
	}
	
	return true;
}

// Get data
void * map_get(map_t const * map, void const * key)
{
	if (map == NULL || key == NULL)
	{
		return NULL;
	}

	map_elem_t elem;
	for (elem = map_first(map); map_continue(map, elem); elem = map_next(elem))
	{
		void * item = map_data(map, elem);

		if (map->equality(key, item))
		{
			return item;
		}
	}

	return NULL;
}

