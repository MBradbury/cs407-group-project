#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "contiki-net.h"

#include "sensor-converter.h"
#include "predicate-checker.h"

#define ERROR_MESSAGE_MAX_LENGTH 96

static struct mesh_conn tc;

// This is the address of the node we are sending
// the data and packets to.
static rimeaddr_t destination;

typedef enum
{
	collect_message_type,
	error_message_type,
	local_data_req_type,
	local_data_resp_type
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
	// This is a message_type_t, but a uint8_t is
	// used for message size optimisation
	uint8_t type;
} base_msg_t;

/** The structure of the message we are sending */
typedef struct
{
	base_msg_t base;

	double temperature;
	double humidity;

	// True if a predicate was violated
	bool pred_violated;

} collect_msg_t;

typedef struct
{
	base_msg_t base;

	// The length of the string in contents
	size_t length;

	// The error message containing predicate
	// failure details
	char contents[ERROR_MESSAGE_MAX_LENGTH];

} error_msg_t;

typedef struct
{
	base_msg_t base;

} local_data_req_msg_t;

typedef struct
{
	base_msg_t base;

	double temperature;
	double humidity;

} local_data_resp_msg_t;


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
	
	mesh_send(&tc, &destination);
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
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 2],
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 1],
		msg->contents, msg->length);

	mesh_send(&tc, &destination);
}


/** The function that will be executed when a message is received */
static void recv(struct mesh_conn * c, rimeaddr_t const * from, uint8_t hops)
{
	base_msg_t const * bmsg = (base_msg_t const *)packetbuf_dataptr();

	switch (bmsg->type)
	{
		case collect_message_type:
		{
			collect_msg_t const * msg = (collect_msg_t const *)bmsg;

			printf("Network Data: Addr:%d.%d Hops:%u Temp:%d Hudmid:%d%% Vio:%s\n",
				from->u8[sizeof(rimeaddr_t) - 2], from->u8[sizeof(rimeaddr_t) - 1],
				hops,
				(int)msg->temperature, (int)msg->humidity,
				msg->pred_violated ? "True" : "False"
			);
		} break;

		case error_message_type:
		{
			error_msg_t const * msg = (error_msg_t const *)bmsg;

			printf("Error occured on %d.%d Hops:%u: %s\n",
				from->u8[sizeof(rimeaddr_t) - 2], from->u8[sizeof(rimeaddr_t) - 1],
				hops, msg->contents
			);

		} break;

		case local_data_req_type:
		{
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
			msg->temperature = temperature;
			msg->humidity = humidity;
	
			mesh_send(&tc, from);
		} break;

		case local_data_resp_type:
		{
			// TODO: Send received data to predicates
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
static void sent(struct mesh_conn * c)
{
	printf("Sent Packet on %u.%u\n",
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 2],
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 1]);
}

/** Called when a send times-out */
static void timeout(struct mesh_conn * c)
{
	printf("Packet Timeout on %u.%u\n",
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 2],
		rimeaddr_node_addr.u8[sizeof(rimeaddr_t) - 1]);
}

static const struct mesh_callbacks callbacks = { &recv, &sent, &timeout };

 
PROCESS(data_collector_process, "Data Collector");
PROCESS(predicate_checker_process, "Predicate Checker");

AUTOSTART_PROCESSES(&data_collector_process, &predicate_checker_process);
 
PROCESS_THREAD(data_collector_process, ev, data)
{
	static struct etimer et;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	mesh_open(&tc, 147, &callbacks);

	// Set to be the sink if the address is 1.0
	memset(&destination, 0, sizeof(rimeaddr_t));
	destination.u8[sizeof(rimeaddr_t) - 2] = 1;

	// Generate data every 20 seconds
	etimer_set(&et, 20 * CLOCK_SECOND);
 
	while (true)
	{
		PROCESS_WAIT_EVENT();

		// Read raw sensor data
		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		// Convert to human understandable values
		double temperature = sht11_temperature(raw_temperature);
		double humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);

		bool violated = false;

		// Check predicates
		//violated |= check_predicate(&temperature_validator, &temperature_message, &temperature);
		//violated |= check_predicate(&humidity_validator, &humidity_message, &humidity);

		// Send data message
		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		debug_packet_size(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();
		memset(msg, 0, sizeof(collect_msg_t));

		msg->base.type = collect_message_type;
		msg->temperature = temperature;
		msg->humidity = humidity;
		msg->pred_violated = violated;
	
		mesh_send(&tc, &destination);

		etimer_reset(&et);
	}
 
exit:
	printf("Exiting data collector process...");
	mesh_close(&tc);
	PROCESS_END();
}


PROCESS_THREAD(predicate_checker_process, ev, data)
{
	static struct etimer et;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	// Generate data every 20 seconds
	etimer_set(&et, 60 * CLOCK_SECOND);
 
	while (true)
	{
		PROCESS_WAIT_EVENT();

		// Read raw sensor data
		SENSORS_ACTIVATE(sht11_sensor);
		unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
		SENSORS_DEACTIVATE(sht11_sensor);

		// Convert to human understandable values
		double temperature = sht11_temperature(raw_temperature);
		double humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);

		// Check predicates
		check_predicate(&temperature_validator, &temperature_message, &temperature);
		check_predicate(&humidity_validator, &humidity_message, &humidity);

		etimer_reset(&et);
	}
 
exit:
	printf("Exiting predicate checker process...");
	PROCESS_END();
}

