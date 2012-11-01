/**
 * This file was mostly lifted from 
 * contiki-2.6/examples/sky/sky-collect.c
 */

#include "contiki.h"

#include <stdio.h>
#include <stdbool.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "net/rime/collect-neighbor.h"
#include "net/rime/timesynch.h"
#include "contiki-net.h"


static struct collect_conn tc;

// The number of retransmits to perform
static const int NORMAL_REXMITS = 4;
static const int ERROR_REXMITS = 10;

#define ERROR_MESSAGE_MAX_LENGTH 128



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
	message_type_t type;
} base_msg_t;

/** The structure of the message we are sending */
typedef struct
{
	base_msg_t base;

	double temperature;
	double humidity;

} collect_msg_t;

typedef struct
{
	base_msg_t base;

	char contents[ERROR_MESSAGE_MAX_LENGTH + 1];
	size_t length;

} error_msg_t;



typedef bool (*predicate_checker_t)(void const *);
typedef void (*predicate_failure_message_t)(void const *);

static void check_predicate(
	predicate_checker_t predicate,
	predicate_failure_message_t message,
	void const * state)
{
	bool result;

	result = (*predicate)(state);

	if (!result)
	{
		(*message)(state);
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
	error_msg_t * msg = (error_msg_t *)packetbuf_dataptr();

	msg->base.type = error_message_type;

	msg->length = snprintf(msg->contents, ERROR_MESSAGE_MAX_LENGTH, "Temperature predicate check failed (0 < T <= 40) where T=%d\n", (int)temperature);
	
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
	error_msg_t * msg = (error_msg_t *)packetbuf_dataptr();

	msg->base.type = error_message_type;

	msg->length = snprintf(msg->contents, ERROR_MESSAGE_MAX_LENGTH, "Humidity predicate check failed (0 < H <= 100) where H=%d%%\n", (int)humidity);
	
	collect_send(&tc, ERROR_REXMITS);
}


static double sht11_relative_humidity(unsigned raw)
{
	// FROM:
	// http://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/Humidity/Sensirion_Humidity_SHT1x_Datasheet_V5.pdf
	// Page 8

	// 12-bit
	static const double c1 = -2.0468;
	static const double c2 = 0.0367;
	static const double c3 = -1.5955E-6;

	// 8-bit
	/*static const double c1 = -2.0468;
	static const double c2 = 0.5872;
	static const double c3 = -4.0845E-4;*/

	return c1 + c2 * raw + c3 * raw * raw;
}

static double sht11_relative_humidity_compensated(unsigned raw, double temperature)
{
	// 12-bit
	static const double t1 = 0.01;
	static const double t2 = 0.00008;

	// 8-bit
	/*static const double t1 = 0.01;
	static const double t2 = 0.00128;*/

	double humidity = sht11_relative_humidity(raw);

	humidity = (temperature - 25) * (t1 + t2 * raw) + humidity;

	//if (humidity > 99)
	//	humidity = 100;

	return humidity;
}

/** Output temperature in degrees Celcius */
static double sht11_temperature(unsigned raw)
{
	//static const double d1 = -40.1; // 5V
	//static const double d1 = -39.8; // 4V
	//static const double d1 = -39.7; // 3.5V
	static const double d1 = -39.6; // 3V
	//static const double d1 = -39.4; // 2.5V

	static const double d2 = 0.01; // 14-bit
	//static const double d2 = 0.04; // 12-bit

	return d1 + d2 * raw;
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

			printf("Network Data: Addr:%d.%d Seqno:%u Hops:%u Temp:%d Hudmid:%d%%\n",
				originator->u8[0], originator->u8[1],
				seqno, hops,
				(int)msg->temperature, (int)msg->humidity
			);
		} break;

		case error_message_type:
		{
			error_msg_t const * msg = (error_msg_t const *)bmsg;

			printf("Error occured on %d.%d: %s",
				originator->u8[0], originator->u8[1],
				msg->contents
			);

		} break;

		default:
		{
			printf("Unknown message type %u (%s)\n", bmsg->type, message_type_to_string(bmsg->type));
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
	static unsigned raw_humidity, raw_temperature;
	static double humidity, temperature;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	collect_open(&tc, 128, COLLECT_ROUTER, &callbacks);

	// Set to be the sink if the address is 1.0
	if (rimeaddr_node_addr.u8[0] == 1 &&
		rimeaddr_node_addr.u8[1] == 0)
	{
		collect_set_sink(&tc, 1);
	}

	etimer_set(&et, 15 * CLOCK_SECOND);
 
	while (true)
	{
		PROCESS_YIELD();

		SENSORS_ACTIVATE(sht11_sensor);

		raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

		SENSORS_DEACTIVATE(sht11_sensor);

		/* http://arduino.cc/forum/index.php?topic=37752.5;wap2 */

		temperature = sht11_temperature(raw_temperature);
		humidity = sht11_relative_humidity_compensated(raw_humidity, temperature);

		check_predicate(&temperature_validator, &temperature_message, &temperature);
		check_predicate(&humidity_validator, &humidity_message, &humidity);


		packetbuf_clear();
		packetbuf_set_datalen(sizeof(collect_msg_t));
		collect_msg_t * msg = (collect_msg_t *)packetbuf_dataptr();

		msg->base.type = collect_message_type;
		msg->temperature = temperature;
		msg->humidity = humidity;
		
		collect_send(&tc, NORMAL_REXMITS);

		etimer_reset(&et);
	}
 
exit:
	collect_close(&tc);
	PROCESS_END();
}

