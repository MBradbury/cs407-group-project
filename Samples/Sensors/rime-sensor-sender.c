/**
 * This file was mostly lifted from 
 * contiki-2.6/examples/sky/sky-collect.c
 */

#include "contiki.h"

#include <stdio.h>

#include "lib/sensors.h"
#include "dev/sht11.h"
#include "dev/sht11-sensor.h"

#include "net/netstack.h"
#include "net/rime.h"
#include "net/rime/collect.h"
#include "net/rime/collect-neighbor.h"
#include "net/rime/timesynch.h"
#include "contiki-net.h"

double sht11_relative_humidity(unsigned raw)
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

double sht11_relative_humidity_compensated(unsigned raw, double temperature)
{
	// 12-bit
	static const double t1 = 0.01;
	static const double t2 = 0.00008;

	// 8-bit
	/*static const double t1 = 0.01;
	static const double t2 = 0.00128;*/

	double humidity = sht11_relative_humidity(raw);

	return (temperature - 25) * (t1 + t2 * raw) + humidity;
}

/** Output temperature in degrees Celcius */
double sht11_temperature(unsigned raw)
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


/** The structure of the message we are sending */
typedef struct collect_msg
{
	double temperature;
	double humidity;

} collect_msg_t;

/** The function that will be executed when a message is received */
static void recv(rimeaddr_t const * originator, uint8_t seqno, uint8_t hops)
{
	collect_msg_t const * msg;

	msg = (collect_msg_t const *)packetbuf_dataptr();

	printf("Network Data: Addr:%d.%d Seqno:%u Hops:%u Temp:%d Hudmid:%d%%\n",
		originator->u8[0], originator->u8[1],
		seqno, hops,
		(int)msg->temperature, (int)msg->humidity
	);
}

/** List of all functions to execute when a message is received */
static const struct collect_callbacks callbacks = { recv };

static struct collect_conn tc;

static const int REXMITS = 4;

 
PROCESS(data_collector_process, "Data Collector");

AUTOSTART_PROCESSES(&data_collector_process);
 
PROCESS_THREAD(data_collector_process, ev, data)
{
	static struct etimer et;
	static unsigned raw_humidity, raw_temperature;
 
	PROCESS_EXITHANDLER(goto exit;)
	PROCESS_BEGIN();

	collect_open(&tc, 128, COLLECT_ROUTER, &callbacks);

	// Set to be the sink if the address is 1.0
	if (rimeaddr_node_addr.u8[0] == 1 &&
		rimeaddr_node_addr.u8[1] == 0)
	{
		collect_set_sink(&tc, 1);
	}

	etimer_set(&et, CLOCK_SECOND);
 
	while (1)
	{
		collect_msg_t * msg;

		PROCESS_YIELD();

		packetbuf_clear();
		msg = (collect_msg_t *)packetbuf_dataptr();
		packetbuf_set_datalen(sizeof(collect_msg_t));

		SENSORS_ACTIVATE(sht11_sensor);

		raw_temperature = sht11_sensor.value(SHT11_SENSOR_TEMP);
		raw_humidity = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

		SENSORS_DEACTIVATE(sht11_sensor);

		/* http://arduino.cc/forum/index.php?topic=37752.5;wap2 */

		msg->temperature = sht11_temperature(raw_temperature);
		msg->humidity = sht11_relative_humidity_compensated(raw_humidity, msg->temperature);
		

		//printf("Temperature: %d degrees Celsius (Raw: %u)\n", (int)msg->temperature, raw_temperature);
		//printf("Humidity: %d%% (Raw: %u)\n", (int)msg->humidity, raw_humidity);


		etimer_reset(&et);

		collect_send(&tc, REXMITS);
	}
 
exit:
	collect_close(&tc);
	PROCESS_END();
}

