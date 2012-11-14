#include <stdio.h>
#include <stdlib.h>
#import "LinkedList.h"

static int size(LinkedList_t const *list)
{
	LinkedListElem_t *node = list->first;

	int count = 0;

	if(node)count++; else return 0;

	while (node->next) 
	{
		node = (LinkedListElem_t *)(node->next);
		count++;
	}
	
	return count;
}

#include <stdio.h>
int main(int argc, char const *argv[])
{
	LinkedList_t *list = (LinkedList_t *)malloc(sizeof(LinkedList_t));

	LinkedListElem_t *first = (LinkedListElem_t *)malloc(sizeof(LinkedListElem_t));
	first->hops = 2;

	list->first = first;

	LinkedListElem_t *node = first;
	int x;
	for (x = 0; x < 20; x++) {
		
		LinkedListElem_t *tmp = (LinkedListElem_t *)malloc(sizeof(LinkedListElem_t));
		node->next = (struct LinkedListElem_t *)tmp;

		node = tmp;
	}

	printf("%d\n",size(list) );

	free(first);
	free(list);

	return 0;
}