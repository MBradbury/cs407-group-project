#include "sensor-converter.h"

double sht11_relative_humidity(int raw)
{
	// FROM:
	// http://www.sensirion.com/fileadmin/user_upload/customers/sensirion/Dokumente/Humidity/Sensirion_Humidity_SHT1x_Datasheet_V5.pdf
	// Page 8

	// Values over 99% indicate fully saturated air, values should be treated as 100%

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

double sht11_relative_humidity_compensated(int raw, double temperature)
{
	// 12-bit
	static const double t1 = 0.01;
	static const double t2 = 0.00008;

	// 8-bit
	/*static const double t1 = 0.01;
	static const double t2 = 0.00128;*/

	double humidity = sht11_relative_humidity(raw);

	humidity = (temperature - 25) * (t1 + t2 * raw) + humidity;

	// When the humidity is greater than 99% treat it as 100%
	if (humidity > 99)
		humidity = 100;

	return humidity;
}

/** Output temperature in degrees Celcius */
double sht11_temperature(int raw)
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

/** From: www.scribd.com/doc/73156710/Contiki-1 */
double battery_voltage(int raw)
{
	return (raw * 2.500 * 2.0) / 4096;
}

