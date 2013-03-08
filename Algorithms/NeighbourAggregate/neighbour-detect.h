#ifndef CS407_NEIGHBOUR_DETECT
#define CS407_NEIGHBOUR_DETECT

#include "unique-array.h"
#include "net/rime/neighbor-discovery.h"

#include <stdint.h>

typedef struct  
{
	//called after a round has been completed, with the latest results for a round
    void (* round_complete_callback)(void );
} neighbour_detect_callbacks_t;

typedef struct neighbour_detect_conn
{
	struct neighbor_discovery_conn nd;
	unique_array_t  results;
	unique_array_t * results_ptr;
	neighbour_detect_callbacks_t callbacks;

} neighbour_detect_conn;

void start_neighbour_detect(neighbour_detect_conn * conn, uint16_t channel, neighbour_detect_callbacks_t cb_fns);
void stop_neighbour_detect(void);

#endif /*CS407_NEIGHBOUR_DETECT*/
