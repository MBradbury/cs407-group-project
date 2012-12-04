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

// Returns the oldest message in the log
log_info_t * log_start(void);

// Gets the next message in the log.
// Returns NULL if there are no messages left to consume.
log_info_t * log_next(log_info_t * log);

void log_message(
	clock_time_t time,
	char const * msg_type, char const * send_type,
	rimeaddr_t const * from, rimeaddr_t const * to);

#endif /*CS407_LOGGING_H*/

