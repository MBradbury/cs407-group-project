#include "Node.h"

//Methods
static void
delayed_send_evaluated_predicate(void *ptr);


//CREATE MESSAGE LIST DATA STRUCTURE
LIST(message_list);

static bool
is_base(void)
{
    static rimeaddr_t base;
    memset(&base, 0, sizeof(rimeaddr_t));
    base.u8[sizeof(rimeaddr_t) - 2] = 1;

    return rimeaddr_cmp(&rimeaddr_node_addr, &base) != 0;
}

static uint8_t
get_message_id(void)
{
    uint8_t returnvalue;
    returnvalue = message_id++;
    returnvalue *= 100;
    returnvalue += rimeaddr_node_addr.u8[0];
    return returnvalue;
}

//STUBBORN BROADCAST
static void
stbroadcast_recv(struct stbroadcast_conn *c)
{
    //Copy Packet Buffer To Memory
    char tmpBuffer[PACKETBUF_SIZE];
    packetbuf_copyto(tmpBuffer);
    predicate_check_msg_t *msg = (predicate_check_msg_t *)tmpBuffer;

    //  printf("I just recieved a Stubborn Broadcast Message! Originator: %s Message: %s Hop: %d Message ID: %d\n",
    //      addr2str(&msg->originator),
    //      msg->predicate_to_check,
    //      msg->hop_limit,
    //      msg->message_id);

    // Check message has not been recieved before
    bool deliver_msg = false;
    struct list_elem_struct *list_iterator = NULL;
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
        struct list_elem_struct *delivered_msg = (struct list_elem_struct *)malloc(sizeof(struct list_elem_struct));
        rimeaddr_copy(&delivered_msg->originator, &msg->originator);
        delivered_msg->message_id = msg->message_id;
        delivered_msg->hops = msg->hop_limit;
        delivered_msg->predicate_to_check = msg->predicate_to_check;
        list_push(message_list, delivered_msg);

        deliver_msg = true;
    }

    //Respond To
    if (deliver_msg)
    {
        //Send predicate value back to originator

        static struct ctimer runicast_timer;
        ctimer_set(&runicast_timer, 21 * CLOCK_SECOND, &delayed_send_evaluated_predicate, &msg->message_id);

        //Rebroadcast Message If Hop Count Is Greater Than 1
        if (msg->hop_limit > 1) //last node
        {
            //Broadcast Message On
            send_n_hop_predicate_check(&rimeaddr_node_addr, msg->message_id, msg->predicate_to_check, msg->hop_limit - 1);
        }
    }
}

static void
stbroadcast_sent(struct stbroadcast_conn *c)
{
    //printf("I've sent!\n");
}

static void
stbroadcast_callback_cancel(void *ptr)
{
    printf("Canceling Stubborn Broadcast.\n");
    stbroadcast_cancel(&stbroadcast);
}

//TODO need to collate responses, then send the on, right now messages collide while a node is trying to forward
//RELIABLE UNICAST
static void
runicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
    printf("runicast received from %s\n", addr2str(from));

    //when recieve message, forward the message on to the originator
    //if the final originator, do something with the value

    //Copy Packet Buffer To Memory
    char tmpBuffer[PACKETBUF_SIZE];
    packetbuf_copyto(tmpBuffer);
    predicate_return_msg_t *msg = (predicate_return_msg_t *)tmpBuffer;


    struct list_elem_struct *list_iterator = NULL;
    for ( list_iterator = (struct list_elem_struct *)list_head(message_list);
            list_iterator != NULL;
            list_iterator = (struct list_elem_struct *)list_item_next(&list_iterator)
        )
    {
        if (list_iterator->message_id == msg->message_id)
        {
            if (rimeaddr_cmp(&rimeaddr_node_addr, &list_iterator->originator)) //if this node was the one who sent the message
            {
                printf("End of forwarding Value: %s received from: %s\n", msg->evaluated_predicate, addr2str(&msg->sender));
            }
            else
            {
                rimeaddr_t dest;
                rimeaddr_copy(&dest, &list_iterator->originator);
                printf("Trying to forward evaluated predicate to: %s\n", addr2str(&dest));

                send_evaluated_predicate(&msg->sender,
                                         &dest,
                                         list_iterator->message_id,
                                         msg->evaluated_predicate
                                        );
            }
            break;
        }
    }

    if (list_iterator == NULL)
    {
        printf("DEBUG: ERROR - LIST IS NULL, THIS IS VERY VERY BAD\n");
    }

}

static void
runicast_sent(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
    //printf("runicast sent\n");
}

static void
runicast_timedout(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
    printf("Runicast timed out to:%s retransmissions:%d\n", addr2str(to), retransmissions);
}


//METHODS
static void
delayed_send_evaluated_predicate(void *ptr)
{
    uint8_t message_id = *(uint8_t *)ptr;
    struct list_elem_struct *list_iterator = NULL;
    for ( list_iterator = (struct list_elem_struct *)list_head(message_list);
            list_iterator != NULL;
            list_iterator = (struct list_elem_struct *)list_item_next(&list_iterator)
        )
    {
        if (list_iterator->message_id == message_id)
        {
            rimeaddr_t dest;
            rimeaddr_copy(&dest, &list_iterator->originator);

            send_evaluated_predicate(&rimeaddr_node_addr,
                                     &dest,
                                     list_iterator->message_id,
                                     evaluate_predicate(&list_iterator->predicate_to_check)
                                    );
            //TODO remove item from the list
            break;
        }
    }

    if (list_iterator == NULL)
    {
        printf("DEBUG: ERROR - LIST IS NULL, THIS IS VERY BAD\n");
    }
}

static char *evaluate_predicate(char const *predicate)
{
    return "Value";
}

static void
delayed_forward_evaluated_predicate(void *ptr)
{
    predicate_return_msg_t *msg = (predicate_return_msg_t *)ptr;
    rimeaddr_t target;
    rimeaddr_copy(&target, &msg->target_reciever);
    rimeaddr_t sender;
    rimeaddr_copy(&sender, &msg->sender);

    send_evaluated_predicate(&sender, &target, msg->message_id, msg->evaluated_predicate);

    free(msg);
}

static void
send_evaluated_predicate(rimeaddr_t const *sender, rimeaddr_t const *target_reciever, uint8_t message_id, char const *evaluated_predicate)
{

    if (runicast_is_transmitting(&runicast))
    {
        printf("runicast is already transmitting, trying again in a few seconds\n");
        static struct ctimer forward_timer;

        predicate_return_msg_t *forwarder = (predicate_return_msg_t *)malloc(sizeof(predicate_return_msg_t));
        rimeaddr_copy(&forwarder->sender, sender);
        rimeaddr_copy(&forwarder->target_reciever, target_reciever);
        forwarder->message_id = message_id;
        forwarder->evaluated_predicate = evaluated_predicate;

        ctimer_set(&forward_timer, 5 * CLOCK_SECOND, &delayed_forward_evaluated_predicate, forwarder);

        return;
    }

    packetbuf_clear();
    packetbuf_set_datalen(sizeof(predicate_return_msg_t));
    debug_packet_size(sizeof(predicate_return_msg_t));
    predicate_return_msg_t *msg = (predicate_return_msg_t *)packetbuf_dataptr();
    memset(msg, 0, sizeof(predicate_return_msg_t));

    rimeaddr_copy(&msg->sender, sender);
    rimeaddr_copy(&msg->target_reciever, target_reciever);
    msg->message_id = message_id;
    msg->evaluated_predicate = evaluated_predicate;


    runicast_send(&runicast, target_reciever, 10);

}



static void
send_n_hop_predicate_check(rimeaddr_t const *originator, uint8_t message_id_to_send, char const *pred, uint8_t hop_limit)
{
    packetbuf_clear();
    packetbuf_set_datalen(sizeof(predicate_check_msg_t));
    debug_packet_size(sizeof(predicate_check_msg_t));
    predicate_check_msg_t *msg = (predicate_check_msg_t *)packetbuf_dataptr();
    memset(msg, 0, sizeof(predicate_check_msg_t));

    rimeaddr_copy(&msg->originator, originator);
    msg->message_id = message_id_to_send;
    msg->predicate_to_check = pred;
    msg->hop_limit = hop_limit;

    random_init(rimeaddr_node_addr.u8[0] + 2);
    int random = (random_rand() % 5);
    if (random <= 1) random++;

    stbroadcast_send_stubborn(&stbroadcast, random * CLOCK_SECOND);

    static struct ctimer stbroadcast_stop_timer;
    ctimer_set(&stbroadcast_stop_timer, 20 * CLOCK_SECOND, &stbroadcast_callback_cancel, NULL);
}

PROCESS(networkInit, "Network Init");
PROCESS(mainProcess, "Main Predicate Checker Process");

AUTOSTART_PROCESSES(&networkInit, &mainProcess);

PROCESS_THREAD(networkInit, ev, data)
{
    static struct etimer et;

    PROCESS_BEGIN();

    random_init(rimeaddr_node_addr.u8[0] + 7);
    int random = (random_rand() % 10);
    if (random <= 1) random++;
    runicast_open(&runicast, 144, &runicastCallbacks);

    // Set the base station
    memset(&baseStationAddr, 0, sizeof(rimeaddr_t));
    baseStationAddr.u8[sizeof(rimeaddr_t) - 2] = 1;


    //5 second timer
    etimer_set(&et, 5 * CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    PROCESS_END();
}

PROCESS_THREAD(mainProcess, ev, data)
{
    static struct etimer et;

    PROCESS_EXITHANDLER(goto exit;)
    PROCESS_BEGIN();

    if (is_base()) //SINK
    {
        leds_on(LEDS_BLUE);

        while (true)
        {
            etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }
    }
    else //NODE
    {
        stbroadcast_open(&stbroadcast, 259, &stbroadcastCallbacks);
        list_init(message_list);

        etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

        static rimeaddr_t test;
        memset(&test, 0, sizeof(rimeaddr_t));
        test.u8[sizeof(rimeaddr_t) - 2] = 2;
        static int count = 0;

        while (true)
        {
            if (rimeaddr_cmp(&rimeaddr_node_addr, &test) && count++ == 0)
            {
                struct list_elem_struct *delivered_msg =  (struct list_elem_struct *)malloc(sizeof(struct list_elem_struct));
                delivered_msg->message_id = get_message_id();
                delivered_msg->hops = 3;
                //set the originator to self
                rimeaddr_copy(&delivered_msg->originator, &rimeaddr_node_addr);
                list_push(message_list, delivered_msg);

                send_n_hop_predicate_check(&rimeaddr_node_addr, delivered_msg->message_id, "Hello World!!!", delivered_msg->hops);
            }

            etimer_set(&et, 10 * CLOCK_SECOND); //10 second timer
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        }
    }

exit:
    printf("Exiting Process...\n");
    runicast_close(&runicast);
    stbroadcast_close(&stbroadcast);
    PROCESS_END();
}
