#!/bin/bash

# Only download contiki if we haven't already
if [ ! -d "~/contiki-2.6" ]; then
	cd /var/tmp
	wget http://downloads.sourceforge.net/project/contiki/Contiki/Contiki%202.6/contiki-2.6.zip

	unzip contiki-2.6.zip -o -d ~
fi
