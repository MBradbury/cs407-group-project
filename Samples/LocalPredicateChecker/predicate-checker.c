#include "predicate-checker.h"

#include "contiki.h"

#include <stdio.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/rime/runicast.h"
#include "net/rime/ipolite.h"
#include "contiki-net.h"

#include "sensor-converter.h"
#include "debug-helper.h"


bool check_predicate(
	predicate_checker_t predicate,
	predicate_failure_message_t message,
	void const * state)
{
	bool result = (*predicate)(state);

	if (!result)
	{
		(*message)(state);
	}

	return result;
}




typedef enum
{
	local_data_req_type,
	local_data_resp_type
} message_type_t;

static const char * message_type_to_string(message_type_t type)
{
	switch (type)
	{
		case local_data_req_type: return "REQ Message";
		case local_data_resp_type: return "RESP Message";
		default: return "Unknown Message";
	}
}

typedef struct
{
	// This is a message_type_t, but a uint8_t is
	// used for message size optimisation
	uint8_t type;
} base_msg_t;

typedef struct
{
	base_msg_t base;

} local_data_req_msg_t;

typedef struct
{
	base_msg_t base;

	data_t data;

} local_data_resp_msg_t;


static const int RETRANSMISSIONS = 25;
static const int MAXDUPS = 6;
static const clock_time_t POLITE_INTERVAL = 5 * CLOCK_SECOND;


static struct ipolite_conn pc;
static struct runicast_conn rc;

static neighbour_predicate_checker_t current_pred_check;
static neighbour_predicate_failure_message_t current_pred_msg;

static void pc_recv(struct ipolite_conn * ptr, rimeaddr_t const * sender)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case local_data_req_type:
		{
			printf("Got req, sending data\n");

			// Read raw sensor data
			SENSORS_ACTIVATE(sht11_sensor);
			unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
			unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
			SENSORS_DEACTIVATE(sht11_sensor);

			// Convert to human understandable values
			double temperature = sht11_temperature(raw_temperature);
			double humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);

			// Send data message
			packetbuf_clear();
			packetbuf_set_datalen(sizeof(local_data_resp_msg_t));
			debug_packet_size(sizeof(local_data_resp_msg_t));
			local_data_resp_msg_t * msg = (local_data_resp_msg_t *)packetbuf_dataptr();
			memset(msg, 0, sizeof(local_data_resp_msg_t));

			msg->base.type = local_data_resp_type;
			msg->data.temperature = temperature;
			msg->data.humidity = humidity;
	
			runicast_send(&rc, sender, RETRANSMISSIONS);
		} break;

		default:
		{
			printf("Unknown message Addr:%d.%d Type:%d (%s)\n",
				sender->u8[sizeof(rimeaddr_t) - 2], sender->u8[sizeof(rimeaddr_t) - 1],
				bmsg->type, message_type_to_string(bmsg->type));
		} break;
	}
}

static const struct ipolite_callbacks pc_callbacks = { &pc_recv, NULL, NULL };




/** The function that will be executed when a message is received */
static void recv(struct runicast_conn * c, rimeaddr_t const * from, uint8_t hops)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case local_data_resp_type:
		{
			local_data_resp_msg_t * msg = (local_data_resp_msg_t *)bmsg;

			printf("Got response, checking predicate against it\n");

			// Evaulate received data in the predicate
			if (!(*current_pred_check)(&msg->data, from))
			{
				(*current_pred_msg)(&msg->data, from);
			}

		} break;

		default:
		{
			printf("Unknown message Addr:%d.%d Hops:%u Type:%d (%s)\n",
				from->u8[sizeof(rimeaddr_t) - 2], from->u8[sizeof(rimeaddr_t) - 1],
				hops,
				bmsg->type, message_type_to_string(bmsg->type));
		} break;
	}
}

/** Called when a packet is sent */
static void sent(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("Sent Packet on %u.%u\n",
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 2],
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 1]);
}

/** Called when a send times-out */
static void timeout(struct runicast_conn * c, rimeaddr_t const * to, uint8_t retransmissions)
{
	printf("Packet Timeout on %u.%u\n",
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 2],
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 1]);
}


static const struct runicast_callbacks rc_callbacks = { &recv, &sent, &timeout };


PROCESS(one_hop_predicate_checker_process, "1-Hop Predicate Checker");

bool check_1_hop_information(
	neighbour_predicate_checker_t predicate,
	neighbour_predicate_failure_message_t message
	)
{
	if (process_is_running(&one_hop_predicate_checker_process))
	{
		printf("1-hop check process is already running, not starting new process\n");

		return false;
	}
	else
	{
		current_pred_check = predicate;
		current_pred_msg = message;

		printf("Starting 1-hop predicate checker process...\n");

		process_start(&one_hop_predicate_checker_process, NULL);

		return true;
	}
}


PROCESS_THREAD(one_hop_predicate_checker_process, ev, data)
{
	static struct etimer et;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	runicast_open(&rc, 118, &rc_callbacks);

	printf("Sending req bcast\n");

	// Send req message
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(local_data_req_msg_t));
	debug_packet_size(sizeof(local_data_req_msg_t));
	local_data_req_msg_t * msg = (local_data_req_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(local_data_req_msg_t));

	msg->base.type = local_data_req_type;

	ipolite_send(&pc, POLITE_INTERVAL, sizeof(local_data_req_msg_t));

	printf("Waiting for responses...\n");

	// Wait for 60 seconds
	etimer_set(&et, 60 * CLOCK_SECOND);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	// We have either received all we are going to
	// and have evaluated the predicate against
	// those responses, so cancel sending.
	ipolite_cancel(&pc);

 
exit:
	printf("Exiting predicate checker process...\n");
	runicast_close(&rc);
	PROCESS_END();
}


void multi_hop_check_start(void)
{
	ipolite_open(&pc, 132, MAXDUPS, &pc_callbacks);
}

void multi_hop_check_end(void)
{
	ipolite_close(&pc);
}



