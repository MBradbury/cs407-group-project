#ifndef CS407_TREE_AGGREGATOR_H
#define CS407_TREE_AGGREGATOR_H

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"

struct tree_agg_conn;

typedef struct
{
	/** The function called when a message is received at the sink.
		The arguments are: the connection and the message source. */
	void (* recv)(struct tree_agg_conn * conn, rimeaddr_t const * source);

	/** This function is called when a node has finished setting up */
	void (* setup_complete)(struct tree_agg_conn * conn);

	void (* aggregate_update)(void * data, void const * to_apply);

	void (* aggregate_own)(void * data);

	void (* store_packet)(struct tree_agg_conn * conn, void const * packet, unsigned int length);

	void (* write_data_to_packet)(struct tree_agg_conn * conn);
} tree_agg_callbacks_t;

typedef struct tree_agg_conn
{
	// DO NOT CHANGE CONNECTION ORDER!!!
	struct stbroadcast_conn bc;
	struct broadcast_conn uc;

	bool has_seen_setup;
	bool is_collecting;
	bool is_leaf_node;

	rimeaddr_t best_parent;
	rimeaddr_t collecting_best_parent;

	rimeaddr_t sink;

	unsigned int best_hop;
	unsigned int collecting_best_hop;

	void * data;
	size_t data_length;

	tree_agg_callbacks_t callbacks;

} tree_agg_conn_t;



bool tree_agg_open(tree_agg_conn_t * conn, rimeaddr_t const * sink,
                   uint16_t ch1, uint16_t ch2,
                   size_t data_size,
                   tree_agg_callbacks_t const * callbacks);

void tree_agg_close(tree_agg_conn_t * conn);

void tree_agg_send(tree_agg_conn_t * conn);

bool tree_agg_is_leaf(tree_agg_conn_t const * conn);
bool tree_agg_is_collecting(tree_agg_conn_t const * conn);

#endif /*CS407_TREE_AGGREGATOR_H*/

