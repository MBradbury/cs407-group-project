#ifndef CS407_MAP
#define CS407_MAP

#include "unique-array.h"

typedef unique_array_t map_t;
typedef unique_array_elem_t map_elem_t;

// Create the list
bool map_init(map_t * map, unique_array_equality_t key_equality, array_list_cleanup_t cleanup);

// Add / Remove items from list
// data should contain the key and the value
bool map_put(map_t * map, void * keyanddata);
bool map_clear(map_t * map);

// data should point to the key
bool map_remove(map_t * map, void const * key);

// Get data
void * map_get(map_t const * map, void const * key);

// Get list length
unsigned int map_length(map_t const * map);

// List iteration 
map_elem_t map_first(map_t const * map);
map_elem_t map_next(map_elem_t elem);
bool map_continue(map_t const * map, map_elem_t elem);
void * map_data(map_t const * map, map_elem_t elem);

// It is recommended that the data structure being used has the key
// as the first item. This allows using map_get with the key directly
// rather than needed to create a temporary key object first.

// Iteration example:
// map_t map;
// map_elem_t elem;
// for (elem = map_first(&map); map_continue(&map, elem); elem = map_next(elem))
// {
//     void * data = map_data(&map, elem);
// }

// Recommended way to add data:
#if 0
typedef struct { int key; int data; } my_map_data_t;

bool map_add(map_t * map, my_map_data_t * data)
{
	if (map == NULL || data == NULL)
		return false;

	my_map_data_t * stored = (my_map_data_t *)map_get(map, &data->key);

	if (stored)
	{
		// Update data
		stored->data = data->data;

		return true;
	}
	else
	{
		// Allocate memory for the data
		my_map_data_t * tostore = malloc(sizeof(my_map_data_t));
		*tostore = *data;

		// Put data in the map
		return map_put(map, data);
	}
}

#endif

#endif

