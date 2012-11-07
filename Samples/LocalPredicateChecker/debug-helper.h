#ifndef DEBUG_HELPER_H
#define DEBUG_HELPER_H

#include <stdbool.h>
#include <string.h>

#include "net/rime.h"

// I was finding that sometimes packets were not
// being set to the correct length. Lets show a
// warning message if they aren't!
bool debug_packet_size(size_t expected);

// NOT THREAD SAFE!
char const * addr2str(rimeaddr_t const * addr);

#endif /*DEBUG_HELPER_H*/

