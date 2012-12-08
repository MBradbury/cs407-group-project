#ifndef CS407_LOGGING_H
#define CS407_LOGGING_H

typedef struct
{
	clock_time_t time;

	rimeaddr_t source;
	rimeaddr_t dest;
	
	char const * msg_type;
	char const * send_type;

} log_info_t;

typedef struct {
	int size;
	int head;
	int count;
	log_info_t *elems;
} message_log

int is_empty(message_log *ml);

// Initialise an empty message log
void log_init(message_log * ml);

// Gets the nth message in the log.
// Returns NULL if there are no messages left to consume.
void * log_read(message_log * ml, log_info_t * log_item, int n);

void log_write(
	message_log * ml, clock_time_t time,
	char const * msg_type, char const * send_type,
	rimeaddr_t const * from, rimeaddr_t const * to);

#endif /*CS407_LOGGING_H*/

