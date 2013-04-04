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

bool unique_array_merge(unique_array_t * first, unique_array_t * second, unique_array_copy_t copy)
{
	if (first == NULL || second == NULL)
	{
		return false;
	}

	array_list_cleanup_t cleanup = (copy == NULL)
		? second->list.cleanup
		: first->list.cleanup;

	bool result = true;

	unique_array_elem_t elem;
	for (elem = unique_array_first(second);
		 unique_array_continue(second, elem);
		 elem = unique_array_next(elem))
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
			// If we are stealing do not attempt a copy as copy will be NULL
			void * item_copy = copy == NULL ? item : copy(item);

			// We have already checked that it is not
			// in the list, so just use the array_list operation
			if (!array_list_append(&first->list, item_copy))
			{
				if (cleanup != NULL)
				{
					// Tidy up the copy we made to prevent memory leaks
					cleanup(item_copy);
				}

				result = false;
			}
		}
		else
		{
			// If we are stealing memory, we need to free the item
			// that we are not going to use
			if (copy == NULL)
			{
				if (cleanup != NULL)
				{
					cleanup(item);
				}
			}
		}
	}

	// We are stealing the memory, so now need to clear the second list
	if (copy == NULL)
	{
		second->list.count = 0;
	}

	return result;
}

// Check if data is in list
bool unique_array_contains(unique_array_t const * list, void const * data)
{
	if (list == NULL || data == NULL)
	{
		return false;
	}

	unique_array_elem_t elem;
	for (elem = unique_array_first(list);
		 unique_array_continue(list, elem);
		 elem = unique_array_next(elem))
	{
		void const * item = unique_array_data(list, elem);

		if (list->equality(data, item))
		{
			return true;
		}
	}

	return false;
}
