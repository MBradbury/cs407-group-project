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


static struct trickle_conn trickle;
static struct stbroadcast_conn stbroadcast;

static rimeaddr_t baseStationAddr;

static uint8_t message_id = 1;

struct list_elem_struct
{
	struct list_elem_struct *next;
	uint8_t message_id;
	uint8_t hops;
};

typedef struct
{
	rimeaddr_t originator;
	uint8_t message_id;
	uint8_t hop_limit;
	char * predicate_to_check;
} predicate_check_msg_t;

typedef struct
{
	rimeaddr_t sender;
	rimeaddr_t target_reciever;
	uint8_t message_id;
	char * evaluated_predicate;
} predicate_return_msg_t;

//Methods
static void 
send_n_hop_predicate_check(rimeaddr_t const * originator, uint8_t message_id, char const * pred, uint8_t hop_limit);
static void
send_predicate_to_node(rimeaddr_t const * sender, rimeaddr_t const * target_reciever, uint8_t const * message_id, char const * evaluated_predicate);
static void
trickle_return_predicate_callback(predicate_return_msg_t const * msg);

static bool 
is_base(void);

//Callbacks
static const struct trickle_callbacks trickleCallbacks = {trickle_recv};
static const struct stbroadcast_callbacks stbroadcastCallbacks = {stbroadcast_recv, stbroadcast_sent};