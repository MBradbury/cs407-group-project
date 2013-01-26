#include "map.h"

#include <stddef.h>
#include <stdlib.h>

bool map_init(map_t * map, unique_array_equality_t key_equality, array_list_cleanup_t cleanup)
{
	if (map == NULL)
		return false;

	return unique_array_init(map, key_equality, cleanup);
}

// Add / Remove items from list
bool map_put(map_t * map, void * data)
{
	return unique_array_append(map, data);
}

bool map_clear(map_t * map)
{
	return unique_array_clear(map);
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

// Get list length
unsigned int map_length(map_t const * map)
{
	return unique_array_length(map);
}

// List iteration 
map_elem_t map_first(map_t const * map)
{
	return unique_array_first(map);
}

map_elem_t map_next(unique_array_elem_t elem)
{
	return unique_array_next(elem);
}

bool map_continue(map_t const * map, map_elem_t elem)
{
	return unique_array_continue(map, elem);
}

void * map_data(map_t const * map, map_elem_t elem)
{
	return unique_array_data(map, elem);
}


