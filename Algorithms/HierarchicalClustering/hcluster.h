#ifndef CS407_HCLUSTER_H
#define CS407_HCLUSTER_H

#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"
#include "net/rime/mesh.h"

struct cluster_conn;

typedef struct
{
	/** The function called when a message is received at the sink.
		The arguments are: the connection and the message source. */
	void (* recv)(struct cluster_conn * conn, rimeaddr_t const * source);

	/** This function is called when a node has finished setting up */
	void (* setup_complete)(struct cluster_conn * conn);
} cluster_callbacks_t;

typedef struct cluster_conn
{
	// DO NOT CHANGE CONNECTION ORDER!!!
	struct stbroadcast_conn bc;
	struct mesh_conn mc;
	struct runicast_conn rc;

	bool has_seen_setup;
	bool is_CH;

	rimeaddr_t our_cluster_head;
	rimeaddr_t collecting_best_CH;

	rimeaddr_t sink;

	unsigned int best_hop;
	unsigned int collecting_best_hop;

	unsigned int head_level;
	unsigned int our_level;
	unsigned int collecting_best_level;

	unsigned int cluster_depth;

	cluster_callbacks_t callbacks;

} cluster_conn_t;



bool cluster_open(cluster_conn_t * conn, rimeaddr_t const * sink,
                  uint16_t ch1, uint16_t ch2, uint16_t ch3,
                  unsigned int cluster_depth, cluster_callbacks_t const * callbacks);

void cluster_close(cluster_conn_t * conn);

void cluster_send(cluster_conn_t * conn);

#endif /*CS407_HCLUSTER_H*/

