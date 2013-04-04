#ifndef CS407_MAP
#define CS407_MAP

#include "containers/unique-array.h"

typedef unique_array_t map_t;
typedef unique_array_elem_t map_elem_t;

// Create the list
#define map_init(map, key_equality, cleanup) unique_array_init(map, key_equality, cleanup)
#define map_free(map) unique_array_free(map)

// Add / Remove items from list
// data should contain the key and the value
#define map_put(map, keyanddata) unique_array_append(map, keyanddata)

#define map_clear(map) unique_array_clear(map)

// data should point to the key
bool map_remove(map_t * map, void const * key);

// Get data
void * map_get(map_t const * map, void const * key);

// Get list length
#define map_length(map) unique_array_length(map)

// List iteration 
#define map_first(map) unique_array_first(map)
#define map_next(elem) unique_array_next(elem)
#define map_continue(map, elem) unique_array_continue(map, elem)
#define map_data(map, elem) unique_array_data(map, elem)


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

