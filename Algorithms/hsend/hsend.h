#ifndef CS407_HSEND_H
#define CS407_HSEND_H

#include "contiki.h"

#include "dev/leds.h"
#include "lib/list.h"

#include "net/rime.h"
#include "net/rime/trickle.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/trickle.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "lib/random.h"
#include "debug-helper.h"


static struct runicast_conn runicast; //connection for trickle
static struct stbroadcast_conn stbroadcast; //connection for the stubborn broadcast

static rimeaddr_t baseStationAddr; //address of the base station

static uint8_t message_id = 1; //initial message ID

//struct for the list elements, used to see if messages have already been sent
struct list_elem_struct
{
    struct list_elem_struct *next;
    rimeaddr_t originator;
    uint8_t message_id;
    uint8_t hops;
    char *predicate_to_check;
};

//struct used to ask other nodes for predicate values
typedef struct
{
    rimeaddr_t originator;
    uint8_t message_id;
    uint8_t hop_limit;
    char *predicate_to_check;
} predicate_check_msg_t;

//struct to send back to the originator with the value of a predicate
typedef struct
{
    rimeaddr_t sender;
    rimeaddr_t target_reciever;
    uint8_t message_id;
    char *evaluated_predicate;
} predicate_return_msg_t;

//Methods
static bool
is_base(void);

static uint8_t
get_message_id(void);

static char *
evaluate_predicate(char const *predicate);

static void
send_evaluated_predicate(rimeaddr_t const *sender, rimeaddr_t const *target_reciever, uint8_t message_id, char const *evaluated_predicate);

static void
send_n_hop_predicate_check(rimeaddr_t const *originator, uint8_t message_id, char const *pred, uint8_t hop_limit);

//RELIABLE UNICAST
static void
runicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno);

static void
runicast_sent(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions);

static void
runicast_timedout(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions);

//STBroadcast
static void
stbroadcast_recv(struct stbroadcast_conn *c);

static void
stbroadcast_sent(struct stbroadcast_conn *c);

static void
stbroadcast_callback_cancel(void *ptr);

//Callbacks
static const struct runicast_callbacks runicastCallbacks = {runicast_recv, runicast_sent, runicast_timedout};
static const struct stbroadcast_callbacks stbroadcastCallbacks = {stbroadcast_recv, stbroadcast_sent};

#endif /*CS407_HSEND_H*/

