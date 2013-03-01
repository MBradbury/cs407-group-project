#ifndef CS407_EVENT_UPDATE_H
#define CS407_EVENT_UPDATE_H

#include <stddef.h>
#include <stdbool.h>

#include "net/rime.h"
#include "net/rime/stbroadcast.h"

typedef void (*data_generation_fn)(void * data);
typedef bool (*data_differs_fn)(void * data1, void * data2);

typedef void (*update_fn)(rimeaddr_t const * from, uint8_t distance, void * data);

typedef struct event_update_conn
{
	// A connection to receive and send data
	struct stbroadcast_conn sc;

	// The distance we will broadcast data out to
	uint8_t distance;

	// A function that retrives the current data state
	data_generation_fn data_fn;

	// Check if data differs
	data_differs_fn differs_fn;

	// The size of the data
	size_t data_size;

	// The data that we last broadcasted
	void * data_loc;

	// This is how often we check that a piece of information has changed
	clock_time_t generate_period;

	// This function is called whenever we receive new information from a node
	update_fn update;

	// The event timer
	ctimer check_timer;

} event_update_conn_t;


bool event_update_start(event_update_conn_t * conn, uint8_t ch, data_generation_fn data_fn, data_differs_fn differs_fn, size_t data_size, clock_time_t generate_period, update_fn update);
void event_update_stop(event_update_conn_t * conn);

void event_update_set_distance(event_update_conn_t * conn, uint8_t distance);

#endif /*CS407_EVENT_UPDATE_H*/
