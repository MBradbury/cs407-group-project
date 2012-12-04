#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/stbroadcast.h"
#include "net/rime/runicast.h"
#include "net/rime/mesh.h"
#include "contiki-net.h"
#include "sys/clock.h"

#include "logging.h"
#include "debug-helper.h"

typedef struct
{
	clock_time_t time;
	rimeaddr_t source;
	rimeaddr_t dest;
	
	char * msg_type;
	char * send_type;

} log_msg_t;

static int number = 0;
static log_msg_t log[10];

log_msg_t * return_log()
{
	return log;
}

void log_message(clock_time_t time, char * msg_type, char * send_type, rimeaddr_t from, rimeaddr_t to)
{
	char from_str[RIMEADDR_STRING_LENGTH];
	char to_str[RIMEADDR_STRING_LENGTH];
	//printf("Logging %s %s message (time %u, pos %d): From %s To %s\n", msg_type, send_type, time, number, addr2str_r(&from, from_str, RIMEADDR_STRING_LENGTH), addr2str_r(&to, to_str, RIMEADDR_STRING_LENGTH));
	printf("%s\n", addr2str_r(&from, from_str, RIMEADDR_STRING_LENGTH));
	printf("%s\n", addr2str_r(&to, to_str, RIMEADDR_STRING_LENGTH));
	printf("%d\n", number);
	log_msg_t msg;
	msg.time = time;
	msg.source = from;
	msg.dest = to;
	msg.msg_type = msg_type;
	msg.send_type = send_type;
	log[number] = msg;
	number = (number + 1) % 10;
}

