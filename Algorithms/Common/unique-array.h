#ifndef CS407_UNIQUE_ARRAY
#define CS407_UNIQUE_ARRAY

#include "array-list.h"

typedef bool (*equality)(void * left, void * right);

typedef struct unique_array
{
	array_list_t list;

	equality_t equality;
} unique_array_t;

typedef array_list_elem_t unique_array_elem_t;

// Create the list
bool unique_array_init(array_list_t * list, equality_t equality, cleanup_t cleanup);

// Add / Remove items from list
bool unique_array_append(unique_array_t * list, void * data);
bool unique_array_clear(unique_array_t * list);

// Check if data is in list
bool unique_array_contains(unique_array_t const * list, void const * data);

// Get list length
unsigned int unique_array_length(unique_array_t const * list);

// List iteration 
unique_array_elem_t unique_array_first(array_list_t const * list);
unique_array_elem_t unique_array_next(array_list_elem_t elem);
bool unique_array_continue(unique_array_t const * list, unique_array_elem_t elem);
void * unique_array_data(unique_array_t const * list, unique_array_elem_t elem);

// Iteration example:
// unique_array_t list;
// unique_array_elem_t elem;
// for (elem = unique_array_first(&list); unique_array_continue(&list, elem); elem = unique_array_next(elem))
// {
//     void * data = unique_array_data(&list, elem);
// }

#endif

