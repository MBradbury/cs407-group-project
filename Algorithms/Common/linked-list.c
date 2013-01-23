#include "linked-list.h"

#include <stdlib.h>
#include <stddef.h>

static linked_list_elem_t * create_elem(void * data)
{
	linked_list_elem_t * e = (linked_list_elem_t *)malloc(sizeof(linked_list_elem_t));
	e->data = data;
	e->next = NULL;

	return e;
}

static linked_list_elem_t * linked_list_last(linked_list_t * list)
{
	if (list == NULL)
	{
		return NULL;
	}
	else
	{
		linked_list_elem_t * elem;
		for (elem = list->head; elem != NULL; elem = elem->next)
		{
			if (elem->next == NULL)
			{
				return elem;
			}
		}

		return NULL;
	}
}

bool linked_list_init(linked_list_t * list, cleanup_t cleanup)
{
	if (list == NULL || cleanup == NULL)
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

	linked_list_elem_t * elem, * prev;
	for (elem = list->head; elem != NULL; )
	{
		if (list->cleanup != NULL)
		{
			list->cleanup(elem->data);
		}

		prev = elem;

		elem = elem->next;

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
		return true;
	}

	linked_list_elem_t * last = linked_list_last(list);
	last->next = create_elem(data);

	return true;
}

unsigned int linked_list_length(linked_list_t * list)
{
	if (list == NULL)
	{
		return 0;
	}

	unsigned int length = 0;

	linked_list_elem_t * elem;
	for (elem = list->head; elem != NULL; elem = elem->next)
	{
		++length;
	}

	return length;
}


linked_list_elem_t * linked_list_first(linked_list_t * list)
{
	return list == NULL ? NULL : list->head;
}

linked_list_elem_t * linked_list_next(linked_list_elem_t * elem)
{
	return elem == NULL ? NULL : elem->next;
}

bool linked_list_continue(linked_list_t const * list, linked_list_elem_t const * elem)
{
	return elem != NULL;
}



