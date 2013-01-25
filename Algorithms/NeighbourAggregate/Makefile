CONTIKI_PROJECT = neighbour-aggregate
all: $(CONTIKI_PROJECT)

# Target references:
# http://www.sics.se/contiki/wiki/index.php/Contiki_make_targets

TARGET = sky

PROJECTDIRS := $(CS407DIR)/Algorithms/Common $(CS407DIR)/Algorithms/TreeAggregator $(CS407DIR)/Algorithms/NeighbourDetect
PROJECT_SOURCEFILES =

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = sensor-converter.c debug-helper.c tree-aggregator.c neighbour-detect.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code \
-Wno-unused-parameter \
-Wuninitialized -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include
