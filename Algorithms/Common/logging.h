#ifndef CS407_LOGGING_H
#define CS407_LOGGING_H

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"
#include "net/rime/mesh.h"

log_msg_t * return_log(void);
void log_message(clock_time_t time, char * msg_type, char * send_type, rimeaddr_t from, rimeaddr_t to);

#endif /*CS407_LOGGING_H*/

