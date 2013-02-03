#include "debug-helper.h"

#include "net/netstack.h"
#include "net/rime.h"

#include <stdio.h>

// I was finding that sometimes packets were not
// being set to the correct length. Lets show a
// warning message if they aren't!
bool debug_packet_size(size_t expected)
{
	bool failed = false;

	uint16_t len = packetbuf_datalen();

	if (len < expected)
	{
		printf("Bad packet length of %u, expected at least %u\n", len, expected);
		failed = true;
	}

	if (len > PACKETBUF_SIZE)
	{
		printf("Packet of length %u is too large, should be %u or lower\n", len, PACKETBUF_SIZE);
		failed = true;
	}

	return failed;
}


char const * addr2str(rimeaddr_t const * addr)
{
	static char str[RIMEADDR_STRING_LENGTH];

	return addr2str_r(addr, str, RIMEADDR_STRING_LENGTH);
}

char const * addr2str_r(rimeaddr_t const * addr, char * str, size_t length)
{
	if (sizeof(rimeaddr_t) == 2)
	{
		snprintf(str, length,
			"%u.%u",
			addr->u8[0], addr->u8[1]
			);
	}
	else
	{
		snprintf(str, length,
			"%u.%u.%u.%u.%u.%u.%u.%u",
			addr->u8[0], addr->u8[1],
			addr->u8[2], addr->u8[3],
			addr->u8[4], addr->u8[5],
			addr->u8[6], addr->u8[7]
			);
	}

	return str;
}

