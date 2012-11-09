typedef struct
{
	int hops;
	int message_id;
	struct LinkedListElem_t * next;

} LinkedListElem_t;

typedef struct
{
	LinkedListElem_t * first;
} LinkedList_t;



static int size(LinkedList_t const * list);