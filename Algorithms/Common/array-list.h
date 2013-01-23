#ifndef CS407_ARRAY_LIST
#define CS407_ARRAY_LIST

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define bool int
#	define true 1
#	define false 0
#endif

typedef void (*cleanup_t)(void * data);

typedef unsigned int array_list_elem_t;

typedef struct array_list
{
	// List memory
	void ** data;

	// Size of reserved memory
	unsigned int length;

	// Number of elements in list
	unsigned int count;

	cleanup_t cleanup;
} array_list_t;

// Create the list
bool array_list_init(array_list_t * list, cleanup_t cleanup);

// Add / Remove items from list
bool array_list_append(array_list_t * list, void * data);
bool array_list_clear(array_list_t * list);

// Get list length
unsigned int array_list_length(array_list_t const * list);

// List iteration 
array_list_elem_t array_list_first(array_list_t const * list);
array_list_elem_t array_list_next(array_list_elem_t elem);
bool array_list_continue(array_list_t const * list, array_list_elem_t elem);
void * array_list_data(array_list_t const * list, array_list_elem_t elem);

// Iteration example:
// array_list_t list;
// array_list_elem_t elem;
// for (elem = array_list_first(&list); array_list_continue(&list, elem); elem = array_list_next(elem))
// {
//     void * data = array_list_data(&list, elem);
// }

#endif

