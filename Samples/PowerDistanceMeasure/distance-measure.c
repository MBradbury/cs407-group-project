/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: example-trickle.c,v 1.5 2010/01/15 10:24:37 nifi Exp $
 */

/**
 * \file
 *         Example for using the trickle code in Rime
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"

#include "dev/leds.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/trickle.h"
#include "contiki-net.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include "dev/cc2420.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>

/*---------------------------------------------------------------------------*/
PROCESS(example_trickle_process, "Trickle example");
PROCESS(button_press_process, "Button Press");
AUTOSTART_PROCESSES(&example_trickle_process, &button_press_process);
/*---------------------------------------------------------------------------*/
static void
trickle_recv(struct trickle_conn *c)
{
	leds_on(LEDS_RED);

	printf("%d.%d: trickle message received '%s'\n",
		rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
		(char *)packetbuf_dataptr());

	clock_wait(CLOCK_SECOND);

	leds_off(LEDS_RED);
}
const static struct trickle_callbacks trickle_call = {trickle_recv};
static struct trickle_conn trickle;
/*---------------------------------------------------------------------------*/

static uint8_t power_level = 1;

PROCESS_THREAD(example_trickle_process, ev, data)
{
	static struct etimer et;

	PROCESS_BEGIN();

	cc2420_set_txpower(power_level);

	trickle_open(&trickle, CLOCK_SECOND, 145, &trickle_call);

	etimer_set(&et, 4 * CLOCK_SECOND);

	while (1)
	{
		leds_off(LEDS_GREEN);

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		leds_on(LEDS_GREEN);

		packetbuf_copyfrom("Hello, world", 13);
		trickle_send(&trickle);

		etimer_reset(&et);
	}

	trickle_close(&trickle);

	PROCESS_END();
}

PROCESS_THREAD(button_press_process, ev, data)
{
	PROCESS_BEGIN();

	SENSORS_ACTIVATE(button_sensor);

	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event &&
			data == &button_sensor);

		++power_level;

		if (power_level > 31)
			power_level = 1;

		printf("New power level %d", power_level);

		cc2420_set_txpower(power_level);
	}

	SENSORS_DEACTIVATE(button_sensor);

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/

