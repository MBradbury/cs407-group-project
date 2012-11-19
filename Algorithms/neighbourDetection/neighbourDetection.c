#include "neighbourDetection.h"

static void
neighbor_discovery_recv(struct neighbor_discovery_conn * c, const rimeaddr_t * from, uint16_t val)
{
	printf("Mote With Address: %s is my Neighbour.\n", addr2str(from));

	bool deliver_msg = false;
	/*struct list_elem_struct * list_iterator = NULL;
	for ( list_iterator = (struct list_elem_struct *)list_head(message_list);
		  list_iterator != NULL;
		  list_iterator = (struct list_elem_struct *)list_item_next(&list_iterator)
		)
	{
		// Message has been delivered before
		if (list_iterator->message_id == msg->message_id)
		{
			// If the new message has a higher hop count
			if (msg->hop_limit > list_iterator->hops)
			{
				printf("Message received before and hops is higher\n");

				//clear the memory, and update the new originator and hop count
				memset(&list_iterator->originator, 0, sizeof(rimeaddr_t));
				rimeaddr_copy(&list_iterator->originator, &msg->originator);

				list_iterator->hops = msg->hop_limit;

				deliver_msg = true;
			}
			break;
		} 
	}
	// End of List and the Message has NOT been delivered before
	if (list_iterator == NULL)
	{
		struct list_elem_struct * delivered_msg = (struct list_elem_struct *)malloc(sizeof(struct list_elem_struct));
		rimeaddr_copy(&delivered_msg->originator,&msg->originator);
		delivered_msg->message_id = msg->message_id;
		delivered_msg->hops = msg->hop_limit;
		delivered_msg->predicate_to_check = msg->predicate_to_check;
		list_push(message_list, delivered_msg);

		deliver_msg = true;
	}*/
}

static void
neighbor_discovery_sent(struct neighbor_discovery_conn * c)
{

}

PROCESS(mainProcess, "Main Neighbour Detection Process");

AUTOSTART_PROCESSES(&mainProcess);

PROCESS_THREAD(mainProcess, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	neighbor_discovery_open(
		&neighborDiscovery,
		5, 
		10 * CLOCK_SECOND, 
		10 * CLOCK_SECOND, 
		60 * CLOCK_SECOND,
		&neighborDiscoveryCallbacks);

	neighbor_discovery_start(&neighborDiscovery, 5);

	while(true)
	{
		etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	}

exit:
	printf("Exiting Process...\n");
	neighbor_discovery_close(&neighborDiscovery);
	PROCESS_END();
}