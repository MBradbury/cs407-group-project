#ifndef CS407_LED_HELPER
#define CS407_LED_HELPER

#include "clock.h"
#include "dev/leds.h"

#ifdef ENABLE_LED_TOGGLE
void toggle_led_for(unsigned char led, clock_time_t time);
#else
#	define toggle_led_for(led, time)
#endif

#endif
