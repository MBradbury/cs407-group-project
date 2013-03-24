#ifndef CS407_UNIQUE_ARRAY
#define CS407_UNIQUE_ARRAY

#include "containers/array-list.h"

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define bool int
#	define true 1
#	define false 0
#endif

typedef bool (*unique_array_equality_t)(void const *, void const *);
typedef void * (*unique_array_copy_t)(void const *);

typedef struct unique_array
{
	array_list_t list;

	unique_array_equality_t equality;
} unique_array_t;

typedef array_list_elem_t unique_array_elem_t;

// Create the list
bool unique_array_init(unique_array_t * list, unique_array_equality_t equality, array_list_cleanup_t cleanup);

#define unique_array_free(ulist) array_list_free(&(ulist)->list)

// Add / Remove items from list
bool unique_array_append(unique_array_t * list, void * data);
bool unique_array_merge(unique_array_t * first, unique_array_t * second, unique_array_copy_t copy);

#define unique_array_clear(ulist) ((ulist) != NULL && array_list_clear(&(ulist)->list))

#define unique_array_remove(ulist, elem) ((ulist) != NULL && array_list_remove(&(ulist)->list, elem))

// Check if data is in list
bool unique_array_contains(unique_array_t const * list, void const * data);

// Get list length
#define unique_array_length(ulist) ((ulist) == NULL ? 0 : array_list_length(&(ulist)->list))

// List iteration 
#define unique_array_first(ulist) ((unique_array_elem_t)0)
#define unique_array_next(elem) array_list_next(elem)
#define unique_array_continue(ulist, elem) ((ulist) != NULL && array_list_continue(&(ulist)->list, elem))
#define unique_array_data(ulist, elem) ((ulist) == NULL ? NULL : array_list_data(&(ulist)->list, elem))

// Iteration example:
// unique_array_t list;
// unique_array_elem_t elem;
// for (elem = unique_array_first(&list); unique_array_continue(&list, elem); elem = unique_array_next(elem))
// {
//     void * data = unique_array_data(&list, elem);
// }

#endif

