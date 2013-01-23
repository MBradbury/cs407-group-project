#ifndef CS407_LINKED_LIST
#define CS407_LINKED_LIST

#include <stdbool.h>

typedef void (*cleanup_t)(void * data);

struct linked_list_elem
{
	void * data;
	struct linked_list_elem * next;
};

typedef struct linked_list_elem * linked_list_elem_t;

typedef struct linked_list
{
	linked_list_elem_t head;
	cleanup_t cleanup;
} linked_list_t;

// Create the list
bool linked_list_init(linked_list_t * list, cleanup_t cleanup);

// Add / Remove items from list
bool linked_list_append(linked_list_t * list, void * data);
bool linked_list_clear(linked_list_t * list);

// Get list length
unsigned int linked_list_length(linked_list_t * list);

// List iteration 
linked_list_elem_t linked_list_first(linked_list_t * list);
linked_list_elem_t linked_list_next(linked_list_elem_t elem);
bool linked_list_continue(linked_list_t const * list, linked_list_elem_t elem);
void * linked_list_data(linked_list_t const * list, linked_list_elem_t elem);

// Iteration example:
// linked_list_t list;
// linked_list_elem_t elem;
// for (elem = linked_list_first(&list); linked_list_continue(&list, elem); elem = linked_list_next(elem))
// {
//     void * data = linked_list_data(&list, elem);
// }

#endif

