#ifndef CS407_NEIGHBOUR_DETECT_H
#define CS407_NEIGHBOUR_DETECT_H

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/unicast.h"

struct n_detect_conn;

typedef struct
{
	/** The function called when a message is received at the sink.
		The arguments are: the connection and the message source. */
	void (* recv)(struct n_detect_conn * conn, rimeaddr_t const * source);

	/** This function is called when a node has finished setting up */
	void (* setup_complete)(struct n_detect_conn * conn);

	void (* detect_update)(void * data, void const * to_apply);

	void (* detect_own)(void * data);
} tree_agg_callbacks_t;

typedef struct n_detect_conn
{
	// DO NOT CHANGE CONNECTION ORDER!!!
	struct stbroadcast_conn bc;
	struct unicast_conn uc;

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

	n_detect_callbacks_t callbacks;

} n_detect_conn_t;



bool n_detect_open(n_detect_conn_t * conn, rimeaddr_t const * sink,
                   uint16_t ch1, uint16_t ch2,
                   size_t data_size,
                   n_detect_callbacks_t const * callbacks);

void n_detect_close(n_detect_conn_t * conn);

void n_detect_send(n_detect_conn_t * conn);

bool n_detect_is_leaf(n_detect_conn_t const * conn);
bool n_detect_is_collecting(n_detect_conn_t const * conn);

#endif /*CS407_NEIGHBOUR_DETECT_H*/

