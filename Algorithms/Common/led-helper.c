#include "led-helper.h"
#include "ctimer.h"

#include <stdlib.h>

typedef struct
{
	struct ctimer ct;
	unsigned char led;
} callbackdata_t;

static void toggle_callback(void * data)
{
	callbackdata_t * ptr = (callbackdata_t *)data;
	leds_toggle(ptr->led);

	free(data);
}

void toggle_led_for(unsigned char led, clock_time_t time)
{
	leds_toggle(led);

	callbackdata_t * ptr = (callbackdata_t *)malloc(sizeof(callbackdata_t));

	ptr->led = led;

	ctimer_set(&ptr->ct, time, &toggle_callback, ptr);
}

