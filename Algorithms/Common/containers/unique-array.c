#include "containers/unique-array.h"

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
	if (list == NULL || data == NULL)
		return false;

	if (!unique_array_contains(list, data))
	{
		if (!array_list_append(&list->list, data))
		{
			if (list->list.cleanup != NULL)
			{
				list->list.cleanup(data);
			}

			return false;
		}
	}
	else
	{
		if (list->list.cleanup != NULL)
		{
			// Free the data given to the unique array
			list->list.cleanup(data);
		}
	}

	// Already in list, so succeeded
	return true;
}

bool unique_array_merge(unique_array_t * first, unique_array_t const * second, unique_array_copy_t copy)
{
	if (first == NULL || second == NULL || copy == NULL)
	{
		return false;
	}

	unique_array_elem_t elem;
	for (elem = unique_array_first(second); unique_array_continue(second, elem); elem = unique_array_next(elem))
	{
		// We cannot just add the item from the second array,
		// as then that item would be owned by two containers.
		//
		// So we need a function that will allocate the memory for
		// the new item and possibly do some conversion.
		void * item = unique_array_data(second, elem);

		if (!unique_array_contains(first, item))
		{
			// We need a clone of this item to put in the other list
			void * item_copy = copy(item);

			// We have already checked that it is not
			// in the list, so just use the array_list operation
			if (!array_list_append(&first->list, item_copy))
			{
				if (first->list.cleanup != NULL)
				{
					// Tidy up the copy we made to prevent memory leaks
					first->list.cleanup(item_copy);
				}

				return false;
			}
		}
	}

	return true;
}

bool unique_array_remove(unique_array_t * list, unique_array_elem_t elem)
{
	return list != NULL && array_list_remove(&list->list, elem);
}

bool unique_array_clear(unique_array_t * list)
{
	return list != NULL && array_list_clear(&list->list);
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
