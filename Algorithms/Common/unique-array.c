#include "unique-array.h"

#include <stdlib.h>
#include <stddef.h>

bool unique_array_init(unique_array_t * list, unique_array_equality_t equality, array_list_cleanup_t cleanup)
{
	if (list == NULL || equality == NULL)
	{
		return false;
	}

	list->equality = equality;

	return array_list_init(&list->list, cleanup);
}

// Add / Remove items from list
bool unique_array_append(unique_array_t * list, void * data)
{
	if (!unique_array_contains(list, data))
	{
		return array_list_append(&list->list, data);
	}

	// Already in list, so succeeded
	return true;
}

bool unique_array_clear(unique_array_t * list)
{
	return list == NULL ? false : array_list_clear(&list->list);
}

// Get list length
unsigned int unique_array_length(unique_array_t const * list)
{
	return list == NULL ? 0 : array_list_length(&list->list);
}

// Check if data is in list
bool unique_array_contains(unique_array_t const * list, void const * data)
{
	if (list == NULL || data == NULL)
	{
		return false;
	}

	unique_array_elem_t elem;
	for (elem = unique_array_first(list); unique_array_continue(list, elem); elem = unique_array_next(elem))
	{
		void const * item = unique_array_data(list, elem);

		if (list->equality(data, item))
		{
			return true;
		}
	}

	return false;
}

// List iteration 
unique_array_elem_t unique_array_first(unique_array_t const * list)
{
	return 0;
}

unique_array_elem_t unique_array_next(unique_array_elem_t elem)
{
	return array_list_next(elem);
}

bool unique_array_continue(unique_array_t const * list, unique_array_elem_t elem)
{
	return list != NULL && array_list_continue(&list->list, elem);
}

void * unique_array_data(unique_array_t const * list, unique_array_elem_t elem)
{
	return list == NULL ? NULL : array_list_data(&list->list, elem);
}



