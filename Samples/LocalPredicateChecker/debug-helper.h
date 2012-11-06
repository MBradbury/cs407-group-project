#ifndef DEBUG_HELPER_H
#define DEBUG_HELPER_H

#include <stdbool.h>
#include <string.h>

// I was finding that sometimes packets were not
// being set to the correct length. Lets show a
// warning message if they aren't!
bool debug_packet_size(size_t expected);

#endif /*DEBUG_HELPER_H*/

