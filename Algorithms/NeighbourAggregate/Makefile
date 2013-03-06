CONTIKI_PROJECT = neighbour-aggregate
all: $(CONTIKI_PROJECT)

PROJECTDIRS := $(CS407DIR)/Algorithms/Common $(CS407DIR)/Algorithms/Common/containers $(CS407DIR)/Algorithms/Common/net
PROJECT_SOURCEFILES =

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = sensor-converter.c debug-helper.c tree-aggregator.c neighbour-detect.c unique-array.c array-list.c led-helper.c random-range.c map.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code -Wmissing-field-initializers \
-Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wno-unused-parameter \
-Wuninitialized -Wold-style-definition -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type

CFLAGS += -Werror=bad-function-cast

CFLAGS += -DNEIGHBOUR_AGG_APPLICATION

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include

