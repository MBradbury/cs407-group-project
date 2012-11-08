#ifndef DEBUG_HELPER_H
#define DEBUG_HELPER_H

#include <stdbool.h>
#include <string.h>

#include "net/rime.h"

// I was finding that sometimes packets were not
// being set to the correct length. Lets show a
// warning message if they aren't!
bool debug_packet_size(size_t expected);

#define RIMEADDR_STRING_LENGTH sizeof(rimeaddr_t) * 4

// Convert a rime address to a string.
// NOT THREAD SAFE!
char const * addr2str(rimeaddr_t const * addr);

// Convert a rime address to a string.
// THREAD SAFE!
char const * addr2str_r(rimeaddr_t const * addr, char * str, size_t length);

#endif /*DEBUG_HELPER_H*/

