#ifndef CS407_TREE_AGGREGATOR_H
#define CS407_TREE_AGGREGATOR_H

#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"

struct tree_agg_conn;

typedef struct
{
	/** The function called when a message is received at the sink.
		The arguments are: the connection and the message source. */
	void (* recv)(struct tree_agg_conn * conn, rimeaddr_t const * source);

	/** This function is called when a node has finished setting up */
	void (* setup_complete)(struct tree_agg_conn * conn);

	void (* aggregate_update)(void * data, void const * to_apply);

	/** This function is used to add a nodes own one data */
	void (* aggregate_own)(void * data);

	/** This function is called when a node needs to savea  packet
		The arguments are: Connection, Packet and the Packet Length  */
	void (* store_packet)(struct tree_agg_conn * conn, void const * packet, unsigned int length);

	/** This function is called to write the nodes stored data to an outward packet */
	void (* write_data_to_packet)(struct tree_agg_conn * conn);
} tree_agg_callbacks_t;

typedef struct tree_agg_conn
{
	// DO NOT CHANGE CONNECTION ORDER!!!
	struct stbroadcast_conn bc;
	struct runicast_conn uc;

	bool has_seen_setup;
	bool is_collecting;
	bool is_leaf_node;

	rimeaddr_t best_parent;

	rimeaddr_t sink;

	unsigned int best_hop;

	void * data;
	size_t data_length;

	tree_agg_callbacks_t callbacks;

	//ctimers
	struct ctimer ctrecv;
	struct ctimer aggregate_ct;
	struct ctimer ct_parent_detect;
	struct ctimer ct_open;
	struct ctimer ct_wait_finished;
} tree_agg_conn_t;



bool tree_agg_open(tree_agg_conn_t * conn, rimeaddr_t const * sink,
                   uint16_t ch1, uint16_t ch2,
                   size_t data_size,
                   tree_agg_callbacks_t const * callbacks);

void tree_agg_close(tree_agg_conn_t * conn);

void tree_agg_send(tree_agg_conn_t * conn);

bool tree_agg_is_sink(tree_agg_conn_t const * conn);
bool tree_agg_is_leaf(tree_agg_conn_t const * conn);
bool tree_agg_is_collecting(tree_agg_conn_t const * conn);

#endif /*CS407_TREE_AGGREGATOR_H*/
