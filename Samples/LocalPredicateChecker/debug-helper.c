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
		printf("Bad packet length of %u, expected at least %u", len, expected);
		failed = true;
	}

	if (len > PACKETBUF_SIZE)
	{
		printf("Packet of length %u is too large, should be %u or lower", len, PACKETBUF_SIZE);
		failed = true;
	}

	return failed;
}

