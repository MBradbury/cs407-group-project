#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "net/rime/broadcast.h"
#include "contiki-net.h"

#include "../Common/sensor-converter.h"
#include "../Common/debug-helper.h"
#include "predicate-checker.h"


static double abs(double d)
{
	return d < 0 ? -d : d;
}



#define ERROR_MESSAGE_MAX_LENGTH 96

static struct mesh_conn mc;

// This is the address of the node we are sending
// the data and packets to.
static rimeaddr_t destination;

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
	// This is a message_type_t, but a uint8_t is
	// used for message size optimisation
	uint8_t type;
} base_msg_t;

/** The structure of the message we are sending */
typedef struct
{
	base_msg_t base;

	// True if a predicate was violated
	bool pred_violated;

	double temperature;
	double humidity;

} collect_msg_t;

typedef struct
{
	base_msg_t base;

	// The error message containing predicate
	// failure details
	char contents[ERROR_MESSAGE_MAX_LENGTH];

	// The length of the string in contents
	size_t length;

} error_msg_t;


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
	
	mesh_send(&mc, &destination);
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

	printf("Sending error message about humidity on %s: %s (%u)\n",
		addr2str(&rimeaddr_node_addr),
		msg->contents, msg->length);

	mesh_send(&mc, &destination);
}


static bool neighbour_humidity_validator(data_t const * value, rimeaddr_t const * sender)
{
	// Read raw sensor data
	SENSORS_ACTIVATE(sht11_sensor);
	unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
	unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	SENSORS_DEACTIVATE(sht11_sensor);

	// Convert to human understandable values
	double temperature = sht11_temperature(raw_temperature);
	double humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);

	// Neighbour's humidity is within 10 of ours
	return abs(humidity - value->humidity) <= 10;
}

static void neighbour_humidity_message(data_t const * value, rimeaddr_t const * sender)
{
	// Read raw sensor data
	SENSORS_ACTIVATE(sht11_sensor);
	unsigned raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
	unsigned raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);
	SENSORS_DEACTIVATE(sht11_sensor);

	// Convert to human understandable values
	double temperature = sht11_temperature(raw_temperature);
	double humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);


	packetbuf_clear();
	packetbuf_set_datalen(sizeof(error_msg_t));
	debug_packet_size(sizeof(error_msg_t));
	error_msg_t * msg = (error_msg_t *)packetbuf_dataptr();
	memset(msg, 0, sizeof(error_msg_t));

	msg->base.type = error_message_type;

	msg->length = snprintf(msg->contents,
		ERROR_MESSAGE_MAX_LENGTH,
		"P1(H) : (|Ho-H1| <= 10) FAILED where Ho=%d%% H1=%d%%",
		(int)humidity, (int)value->humidity);

	printf("Sending error message about 1-hop humidity on %s: %s (%u)\n",
		addr2str(&rimeaddr_node_addr),
		msg->contents, msg->length);

	mesh_send(&mc, &destination);
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

			printf("Network Data: Addr:%s Hops:%u Temp:%d Hudmid:%d%% Vio:%s\n",
				addr2str(from),
				hops,
				(int)msg->temperature, (int)msg->humidity,
				msg->pred_violated ? "True" : "False"
			);
		} break;

		case error_message_type:
		{
			error_msg_t const * msg = (error_msg_t const *)bmsg;

			printf("Error occured on %s Hops:%u: %s\n",
				addr2str(from),
				hops, msg->contents
			);

		} break;

		default:
		{
			printf("Unknown message Addr:%s Hops:%u Type:%d (%s)\n",
				addr2str(from),
				hops,
				bmsg->type, message_type_to_string(bmsg->type));
		} break;
	}
}

/** Called when a packet is sent */
static void sent(struct mesh_conn * c)
{
	printf("Sent Packet on %s\n",
		addr2str(&rimeaddr_node_addr));
}

/** Called when a send times-out */
static void timeout(struct mesh_conn * c)
{
	printf("Packet Timeout on %s\n",
		addr2str(&rimeaddr_node_addr));
}

static const struct mesh_callbacks callbacks = { &recv, &sent, &timeout };
 
PROCESS(data_collector_process, "Data Collector");
PROCESS(predicate_checker_process, "Predicate Checker");

AUTOSTART_PROCESSES(&data_collector_process/*, &predicate_checker_process*/);
 
PROCESS_THREAD(data_collector_process, ev, data)
{
	static struct etimer et;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	mesh_open(&mc, 147, &callbacks);

	// Set to be the sink if the address is 1.0
	memset(&destination, 0, sizeof(rimeaddr_t));
	destination.u8[sizeof(rimeaddr_t) - 2] = 1;

	// Generate data every 20 seconds
	etimer_set(&et, 20 * CLOCK_SECOND);
 
	while (true)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

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
		violated |= check_predicate(&temperature_validator, &temperature_message, &temperature);
		violated |= check_predicate(&humidity_validator, &humidity_message, &humidity);

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
	
		mesh_send(&mc, &destination);

		etimer_reset(&et);
	}
 
exit:
	printf("Exiting data collector process...\n");
	mesh_close(&mc);
	PROCESS_END();
}


PROCESS_THREAD(predicate_checker_process, ev, data)
{
	static struct etimer et;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	multi_hop_check_start();

	// Generate data every 60 seconds
	etimer_set(&et, 80 * CLOCK_SECOND);
 
	while (true)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		printf("Checking 1-hop predicates...\n");

		check_1_hop_information(
			&neighbour_humidity_validator,
			&neighbour_humidity_message);

		etimer_reset(&et);
	}
 
exit:
	printf("Exiting predicate checker process...\n");
	multi_hop_check_end();
	PROCESS_END();
}

