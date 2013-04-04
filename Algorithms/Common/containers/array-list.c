#include "containers/array-list.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

static bool realloc_array_list(array_list_t * list)
{
	if (list == NULL)
	{
		return false;
	}

	if (list->length == 0)
	{
		list->length = 20;
	}
	else
	{
		list->length = (3 * list->length) / 2;
	}

	void ** newdata = (void **)malloc(sizeof(void *) * list->length);

	if (newdata == NULL)
	{
		return false;
	}

	if (list->data != NULL && list->count != 0)
	{
		memcpy(newdata, list->data, sizeof(void *) * list->count);
		free(list->data);
	}

	list->data = newdata;

	return true;
}

bool array_list_init(array_list_t * list, array_list_cleanup_t cleanup)
{
	if (list == NULL)
	{
		return false;
	}

	list->data = NULL;
	list->count = 0;
	list->length = 0;
	list->cleanup = cleanup;

	return true;
}

bool array_list_free(array_list_t * list)
{
	if (list == NULL)
	{
		return false;
	}

	array_list_clear(list);

	free(list->data);
	list->data = NULL;
	list->length = 0;

	return true;
}

bool array_list_clear(array_list_t * list)
{
	if (list == NULL)
	{
		return false;
	}

	array_list_elem_t elem;
	for (elem = array_list_first(list); array_list_continue(list, elem); elem = array_list_next(elem))
	{
		void * data = array_list_data(list, elem);

		if (data != NULL && list->cleanup != NULL)
		{
			list->cleanup(data);
		}
	}

	list->count = 0;

	return true;
}

bool array_list_append(array_list_t * list, void * data)
{
	if (list == NULL || data == NULL)
	{
		return false;
	}

	// Out of space so add some more
	if (list->count == list->length)
	{
		if (!realloc_array_list(list))
		{
			// If we failed to allocate the list, free the data given

			if (list->cleanup != NULL)
			{
				list->cleanup(data);
			}

			return false;
		}
	}

	// Add item
	list->data[list->count] = data;

	list->count += 1;

	return true;
}

bool array_list_remove(array_list_t * list, array_list_elem_t elem)
{
	if (list == NULL || elem >= list->count)
	{
		return false;
	}

	void ** item = list->data + elem;

	// Free memory of item
	if (list->cleanup != NULL)
	{
		list->cleanup(*item);
	}

	void ** next = item + 1;

	// Use memmove to handle overlapping data
	memmove(item, next, (list->count - elem - 1) * sizeof(void **));

	list->count -= 1;

	return true;
}
