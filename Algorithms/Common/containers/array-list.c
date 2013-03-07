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

		if (list->cleanup != NULL)
		{
			list->cleanup(data);
		}
	}

	free(list->data);

	list->data = NULL;
	list->length = 0;
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
		realloc_array_list(list);
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

unsigned int array_list_length(array_list_t const * list)
{
	return list == NULL ? 0 : list->count;
}

array_list_elem_t array_list_first(array_list_t const * list)
{
	return 0;
}

array_list_elem_t array_list_next(array_list_elem_t elem)
{
	return elem + 1;
}

bool array_list_continue(array_list_t const * list, array_list_elem_t elem)
{
	if (list == NULL)
		return false;

	return elem < list->count;
}

void * array_list_data(array_list_t const * list, array_list_elem_t elem)
{
	return list == NULL || elem >= list->count ? NULL : list->data[elem];
}


