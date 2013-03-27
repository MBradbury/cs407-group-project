#ifndef CS407_LINKED_LIST
#define CS407_LINKED_LIST

#ifndef _MSC_VER
#	include <stdbool.h>
#else
#	define bool int
#	define true 1
#	define false 0
#endif

typedef void (*linked_list_cleanup_t)(void * data);

struct linked_list_elem
{
	void * data;
	struct linked_list_elem * next;
};

typedef struct linked_list_elem * linked_list_elem_t;

typedef struct linked_list
{
	linked_list_elem_t head;
	linked_list_cleanup_t cleanup;
} linked_list_t;

// Create the list
bool linked_list_init(linked_list_t * list, linked_list_cleanup_t cleanup);

// linked_list_free is the same as linked_list_clear
#define linked_list_free linked_list_clear

// Add / Remove items from list
bool linked_list_append(linked_list_t * list, void * data);
bool linked_list_clear(linked_list_t * list);

void * linked_list_peek(linked_list_t const * list);
bool linked_list_pop(linked_list_t * list);

// Get list length
unsigned int linked_list_length(linked_list_t const * list);

#ifdef CONTAINERS_CHECKED

#	define linked_list_is_empty(list) \
		((list) != NULL && (list)->head == NULL)

#	define linked_list_first(list) \
		((list) == NULL ? NULL : (list)->head)

#	define linked_list_next(elem) \
		((elem) == NULL ? NULL : (elem)->next)

#	define linked_list_continue(list, elem) \
		((elem) != NULL)

#	define linked_list_data(list, elem) \
		((elem) == NULL ? NULL : (elem)->data)

#else

#	define linked_list_is_empty(list) \
		((list)->head == NULL)

#	define linked_list_first(list) \
		((list)->head)

#	define linked_list_next(elem) \
		((elem)->next)

#	define linked_list_continue(list, elem) \
		((elem) != NULL)

#	define linked_list_data(list, elem) \
		((elem)->data)

#endif

// Iteration example:
// linked_list_t list;
// linked_list_elem_t elem;
// for (elem = linked_list_first(&list); linked_list_continue(&list, elem); elem = linked_list_next(elem))
// {
//     void * data = linked_list_data(&list, elem);
// }

#endif

