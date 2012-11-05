#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "contiki-net.h"

#include "sensor-converter.h"
#include "predicate-checker.h"


// The number of retransmits to perform
static const int NORMAL_REXMITS = 4;
static const int ERROR_REXMITS = 6;

#define ERROR_MESSAGE_MAX_LENGTH 96

static struct collect_conn tc;

typedef enum
{
	collect_message_type,
	error_message_type
} message_type_t;

static const char * message_type_to_string(message_type_t type)
{
	switch (type)
	{
		case collect_message_type: return "Collect Message";
		case error_message_type: return "Error Message";
		default: return "Unknown Message";
	}
}

typedef struct
{
	uint8_t type;
} base_msg_t;

/** The structure of the message we are sending */
typedef struct
{
	base_msg_t base;

	double temperature;
	double humidity;

	bool pred_violated;

} collect_msg_t;

typedef struct
{
	base_msg_t base;

	size_t length;
	char contents[ERROR_MESSAGE_MAX_LENGTH];

} error_msg_t;


// I was finding that sometimes packets were not
// being set to the correct length. Lets show a
// warning message if they aren't!
static void debug_packet_size(size_t expected)
{
	uint16_t len = packetbuf_datalen();

	if (len < expected)
	{
		printf("Bad packet length of %u, expected at least %u", len, expected);
	}

	if (len > PACKETBUF_SIZE)
	{
		printf("Packet of length %u is too large, should be %u or lower", len, PACKETBUF_SIZE);
	}
}


static bool temperature_validator(void const * value)
{
	double temperature = *(double const *)value;

	return temperature > 0 && temperature <= 40;
}

static void temperature_message(void const * value)
{
	double temperature = *(double const *)value;

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(error_msg_t));
	debug_packet_size(sizeof(error_msg_t));
	error_msg_t * msg = (error_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(error_msg_t));

	msg->base.type = error_message_type;

	msg->length = snprintf(msg->contents,
		ERROR_MESSAGE_MAX_LENGTH,
		"P(T) : (0 < T <= 40) FAILED where T=%d",
		(int)temperature);
	
	collect_send(&tc, ERROR_REXMITS);
}

static bool humidity_validator(void const * value)
{
	double humidity = *(double const *)value;

	return humidity > 0 && humidity <= 100;
}

static void humidity_message(void const * value)
{
	double humidity = *(double const *)value;

	packetbuf_clear();
	packetbuf_set_datalen(sizeof(error_msg_t));
	debug_packet_size(sizeof(error_msg_t));
	error_msg_t * msg = (error_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(error_msg_t));

	msg->base.type = error_message_type;

	msg->length = snprintf(msg->contents,
		ERROR_MESSAGE_MAX_LENGTH,
		"P(H) : (0 < H <= 100) FAILED where H=%d%%",
		(int)humidity);

	printf("Sending error message about humidity on %u.%u: %s (%u)\n",
		rimeaddr_node_addr.u8[0],
		rimeaddr_node_addr.u8[1],
		msg->contents, msg->length);

	collect_send(&tc, ERROR_REXMITS);
}


/** The function that will be executed when a message is received */
static void recv(rimeaddr_t const * originator, uint8_t seqno, uint8_t hops)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case collect_message_type:
		{
			collect_msg_t const * msg = (collect_msg_t const *)bmsg;

			printf("Network Data: Addr:%d.%d Seqno:%u Hops:%u Temp:%d Hudmid:%d%% Vio:%s\n",
				originator->u8[0], originator->u8[1],
				seqno, hops,
				(int)msg->temperature, (int)msg->humidity,
				msg->pred_violated ? "True" : "False"
			);
		} break;

		case error_message_type:
		{
			error_msg_t const * msg = (error_msg_t const *)bmsg;

			printf("Error occured on %d.%d Seqno:%u Hops:%u Type:%d (%s): %s\n",
				originator->u8[0], originator->u8[1],
				seqno, hops,
				bmsg->type, message_type_to_string(bmsg->type),
				msg->contents
			);

		} break;

		default:
		{
			printf("Unknown message Addr:%d.%d Seqno:%u Hops:%u Type:%d (%s)\n",
				originator->u8[0], originator->u8[1],
				seqno, hops,
				bmsg->type, message_type_to_string(bmsg->type));
		} break;
	}
}

/** List of all functions to execute when a message is received */
static const struct collect_callbacks callbacks = { recv };

 
PROCESS(data_collector_process, "Data Collector");

AUTOSTART_PROCESSES(&data_collector_process);
 
PROCESS_THREAD(data_collector_process, ev, data)
{
	static struct etimer et;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	collect_open(&tc, 128, COLLECT_ROUTER, &callbacks);

	// Set to be the sink if the address is 1.0
	if (rimeaddr_node_addr.u8[0] == 1 &&
		rimeaddr_node_addr.u8[1] == 0)
	{
		collect_set_sink(&tc, 1);
	}

	// Wait for network to settle
	etimer_set(&et, 120 * CLOCK_SECOND);
	PROCESS_WAIT_UNTIL(etimer_expired(&et));

	etimer_set(&et, 20 * CLOCK_SECOND);
 
	while (true)
	{
		static unsigned raw_humidity, raw_temperature;
		static double humidity, temperature;

		PROCESS_WAIT_EVENT();

		// Read raw sensor data
		SENSORS_ACTIVATE(sht11_sensor);
		raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		// Convert to human understandable
		temperature = sht11_temperature(raw_temperature);
		humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);

		bool violated = false;

		// Check predicates
		violated |= check_predicate(&temperature_validator, &temperature_message, &temperature);
		violated |= check_predicate(&humidity_validator, &humidity_message, &humidity);

		// Send data message
		/*packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		debug_packet_size(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
		memset(msg, 0, sizeof(collect_msg_t));

		msg->base.type = collect_message_type;
		msg->temperature = temperature;
		msg->humidity = humidity;
		msg->pred_violated = violated;
	
		collect_send(&tc, NORMAL_REXMITS);*/

		etimer_reset(&et);
	}
 
exit:
	printf("Exiting process...");
	collect_close(&tc);
	PROCESS_END();
}

