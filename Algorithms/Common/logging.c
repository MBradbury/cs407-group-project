#include "net/netstack.h"
#include "sys/clock.h"

#include "logging.h"
#include "debug-helper.h"

#define LOG_SIZE 10

static unsigned int number = 0;

static log_info_t log[LOG_SIZE];

log_info_t * log_start(void)
{
	return log[number]
}

log_info_t * log_next(log_info_t * log_item)
{
	for (int i = 0; i < LOG_SIZE; i++)
	{
		if (log_item.time < log[(number + i)%LOG_SIZE].time)
			return log[(number + i)%LOG_SIZE];
	}
	return null;
}

void log_message(clock_time_t time, char * msg_type, char * send_type, rimeaddr_t const * from, rimeaddr_t const * to)
{
	char from_str[RIMEADDR_STRING_LENGTH];
	char to_str[RIMEADDR_STRING_LENGTH];

	/*printf("Logging %s %s message (time %u, pos %d): From %s To %s\n", msg_type, send_type, time, number, addr2str_r(&from, from_str, RIMEADDR_STRING_LENGTH), addr2str_r(&to, to_str, RIMEADDR_STRING_LENGTH));
	printf("%s\n", addr2str_r(&from, from_str, RIMEADDR_STRING_LENGTH));
	printf("%s\n", addr2str_r(&to, to_str, RIMEADDR_STRING_LENGTH));
	printf("%d\n", number);*/

	log_msg_t msg;

	msg.time = time;
	msg.source = from;
	msg.dest = to;
	msg.msg_type = msg_type;
	msg.send_type = send_type;

	log[number] = msg;

	number = (number + 1) % LOG_SIZE;
}

