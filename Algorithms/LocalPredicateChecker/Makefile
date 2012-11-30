CONTIKI_PROJECT = local-predicate-checker
all: $(CONTIKI_PROJECT)

# Target references:
# http://www.sics.se/contiki/wiki/index.php/Contiki_make_targets

TARGET = sky

PROJECTDIRS := $(CS407DIR)/Algorithms/Common
PROJECT_SOURCEFILES = predicate-checker.c

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = sensor-converter-broken.c debug-helper.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wfloat-equal

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include
