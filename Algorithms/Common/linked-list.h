#ifndef CS407_LINKED_LIST
#define CS407_LINKED_LIST

#include <stdbool.h>

typedef void (*cleanup_t)(void * data);

typedef struct linked_list_elem
{
	void * data;
	struct linked_list_elem * next;
} linked_list_elem_t;

typedef struct linked_list
{
	linked_list_elem_t * head;
	unsigned int length;
	cleanup_t cleanup;
} linked_list_t;

bool init_linked_list(linked_list_t * list, cleanup_t cleanup);
bool free_linked_list(linked_list_t * list);

bool linked_list_append(linked_list_t * list, void * data);
bool linked_list_clear(linked_list_t * list);

unsigned int linked_list_length(linked_list_t * list);

linked_list_elem_t * linked_list_first(linked_list_t * list);
linked_list_elem_t * linked_list_last(linked_list_t * list);

#endif
