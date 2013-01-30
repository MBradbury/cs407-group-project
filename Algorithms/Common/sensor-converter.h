#ifndef SENSOR_CONVERTER_H
#define SENSOR_CONVERTER_H

double sht11_relative_humidity(int raw);
double sht11_relative_humidity_compensated(int raw, double temperature);
double sht11_temperature(int raw);

double battery_voltage(int raw);

/* Photosynthetically Active Radiation in Lux. */
double s1087_light1(int raw);

/* Total Solar Radiation in Lux. */
double s1087_light2(int raw);

#endif

