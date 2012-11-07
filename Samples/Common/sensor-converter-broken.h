#ifndef SENSOR_CONVERTER_H
#define SENSOR_CONVERTER_H

double sht11_relative_humidity(unsigned raw);
double sht11_relative_humidity_compensated(unsigned raw, double temperature);
double sht11_temperature(unsigned raw);

double battery_voltage(unsigned raw);

#endif
