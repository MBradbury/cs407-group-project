#include "logging.h"
#include "debug-helper.h"

#define LOG_SIZE 10

void log_init(message_log *ml)
{
	ml->size = LOG_SIZE;
	ml->head = 0;
	ml->count = 0;
	//memset(ml->elems, 0, sizeof(log_info_t) * LOG_SIZE);
}

int is_empty(message_log *ml)
{
	return ml->count==0;
}

void log_read(message_log *ml, log_info_t * log_item, int n)
{
	if (n <= ml->head + ml->count)
		log_item = &ml->elems[ml->head+n];
	else
		log_item =  NULL;
}

void log_write(message_log * ml, clock_time_t time, char const * msg_type, char const * send_type, rimeaddr_t const * from, rimeaddr_t const * to)
{
	/*char from_str[RIMEADDR_STRING_LENGTH];
	char to_str[RIMEADDR_STRING_LENGTH];

	printf("Logging %s %s message (time %u, pos %d): From %s To %s\n", msg_type, send_type, time, number, addr2str_r(&from, from_str, RIMEADDR_STRING_LENGTH), addr2str_r(&to, to_str, RIMEADDR_STRING_LENGTH));*/

	log_info_t msg;
	int tail = (ml->head + ml->count) % ml->size;

	msg.time = time;
	msg.source = *from;
	msg.dest = *to;
	msg.msg_type = msg_type;
	msg.send_type = send_type;

	ml->elems[tail] = msg;
	if (ml->count == ml->size)
		ml->head = (ml->head+1) % ml->size;
	else
		ml->count++;
}

