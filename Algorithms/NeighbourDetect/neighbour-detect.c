#include "neighbour-detect.h"

LIST(neighbor_list); //create neighbor list

static void
neighbor_discovery_recv(struct neighbor_discovery_conn * c, const rimeaddr_t * from, uint16_t val)
{
        struct neighbor_list_item * list_iterator = NULL;
        for ( list_iterator = (struct neighbor_list_item *)list_head(neighbor_list);
                  list_iterator != NULL;
                  list_iterator = (struct neighbor_list_item *)list_item_next(list_iterator)
                )
        {
                // Neighbour has been discovered before;
                //printf("%s ", addr2str(from));
                //printf("%s\n", addr2str(&list_iterator->neighbor_rimeaddr));
                if (rimeaddr_cmp(&list_iterator->neighbor_rimeaddr, from))
                {
                        break;
                }

        }
        // End of List and neighbor has not been discovered before
        if (list_iterator == NULL)
        {
                struct neighbor_list_item * neighbor_to_store = (struct neighbor_list_item *)malloc(sizeof(struct neighbor_list_item));
                rimeaddr_copy(&neighbor_to_store->neighbor_rimeaddr, from);
                list_push(neighbor_list, neighbor_to_store);            
                printf("Mote With Address: %s is my Neighbour.\n", addr2str(from));
        }
}

static void
neighbor_discovery_sent(struct neighbor_discovery_conn * c)
{

}

static const struct neighbor_discovery_callbacks neighbor_discovery_callbacks = {neighbor_discovery_recv, neighbor_discovery_sent};

PROCESS(main_process, "Main Neighbour Detection Process");

static void start_neighbour_detect(struct neighbor_list_item* list_ptr)
{
	process_start(&main_process, (void*)list_ptr);
}

PROCESS_THREAD(main_process, ev, data)
{
        static struct etimer et;
        
        PROCESS_EXITHANDLER(goto exit;)
        PROCESS_BEGIN();
		
		struct neighbor_list_item* list_ptr = (struct neighbor_list_item *)data;

        neighbor_discovery_open(
                &neighbor_discovery,
                5, 
                10 * CLOCK_SECOND, 
                10 * CLOCK_SECOND, 
                60 * CLOCK_SECOND,
                &neighbor_discovery_callbacks);

        neighbor_discovery_start(&neighbor_discovery, 5);

        while(true)
        {
                etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }

exit:
        printf("Exiting Process...\n");
        neighbor_discovery_close(&neighbor_discovery);
        PROCESS_END();
}
