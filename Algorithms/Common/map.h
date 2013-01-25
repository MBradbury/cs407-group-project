#ifndef CS407_MAP
#define CS407_MAP

#include "unique-array.h"

typedef unique_array_t map_t;
typedef unique_array_elem_t map_elem_t;

// Create the list
bool map_init(map_t * map, unique_array_elem_t key_equality, array_list_cleanup_t cleanup);

// Add / Remove items from list
// data should contain the key and the value
bool map_put(map_t * map, void * data);
bool map_clear(map_t * map);

// Get data
void * map_get(map_t const * map, void const * key);

// Get list length
unsigned int map_length(map_t const * map);

// List iteration 
map_elem_t map_first(map_t const * map);
map_elem_t map_next(map_elem_t elem);
bool map_continue(map_t const * map, map_elem_t elem);
void * map_data(map_t const * map, map_elem_t elem);

// Iteration example:
// map_t map;
// map_elem_t elem;
// for (elem = map_first(&map); map_continue(&map, elem); elem = map_next(elem))
// {
//     void * data = map_data(&map, elem);
// }

#endif

