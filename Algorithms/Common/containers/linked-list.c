#include "containers/linked-list.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

static linked_list_elem_t create_elem(void * data)
{
	linked_list_elem_t e = (linked_list_elem_t)malloc(sizeof(struct linked_list_elem));

	if (e != NULL)
	{
		e->data = data;
		e->next = NULL;
	}

	return e;
}

static linked_list_elem_t linked_list_last(linked_list_t const * list)
{
	if (list == NULL)
	{
		return NULL;
	}
	else
	{
		linked_list_elem_t elem;
		for (elem = linked_list_first(list); linked_list_continue(list, elem); elem = linked_list_next(elem))
		{
			if (elem->next == NULL)
			{
				return elem;
			}
		}

		return NULL;
	}
}

bool linked_list_init(linked_list_t * list, linked_list_cleanup_t cleanup)
{
	if (list == NULL)
	{
		return false;
	}

	list->head = NULL;
	list->cleanup = cleanup;

	return true;
}

bool linked_list_clear(linked_list_t * list)
{
	if (list == NULL)
	{
		return false;
	}

	linked_list_elem_t elem, prev;
	for (elem = linked_list_first(list); linked_list_continue(list, elem);)
	{
		if (list->cleanup != NULL)
		{
			list->cleanup(elem->data);
		}

		prev = elem;

		elem = linked_list_next(elem);

		free(prev);
	}

	list->head = NULL;

	return true;
}

bool linked_list_append(linked_list_t * list, void * data)
{
	if (list == NULL)
	{
		return false;
	}

	// No head element, so add this as head
	if (list->head == NULL)
	{
		list->head = create_elem(data);

		if (list->head == NULL)
		{
			return false;
		}
	}
	else
	{
		linked_list_elem_t last = linked_list_last(list);

		if (last == NULL)
		{
			return false;
		}

		last->next = create_elem(data);

		if (last->next == NULL)
		{
			return false;
		}
	}

	return true;
}

void * linked_list_peek(linked_list_t const * list)
{
	linked_list_elem_t first = linked_list_first(list);

	return first == NULL ? NULL : first->data;
}

bool linked_list_pop(linked_list_t * list)
{
	if (list == NULL)
	{
		return false;
	}

	linked_list_elem_t first = linked_list_first(list);

	if (first == NULL)
	{
		return false;
	}

	list->head = first->next;

	if (list->cleanup != NULL)
	{
		list->cleanup(first->data);
	}

	free(first);
	
	return true;
}

unsigned int linked_list_length(linked_list_t const * list)
{
	if (list == NULL)
	{
		return 0;
	}

	unsigned int length = 0;

	linked_list_elem_t elem;
	for (elem = linked_list_first(list); linked_list_continue(list, elem); elem = linked_list_next(elem))
	{
		++length;
	}

	return length;
}
