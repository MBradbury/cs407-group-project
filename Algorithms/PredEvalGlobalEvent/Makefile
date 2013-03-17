CONTIKI_PROJECT = pred-eval
all: $(CONTIKI_PROJECT)

PROJECTDIRS := $(CS407DIR)/Algorithms/Common $(CS407DIR)/Algorithms/Common/containers \
$(CS407DIR)/Algorithms/Common/net $(CS407DIR)/PredicateLanguage \
$(CS407DIR)/Algorithms/TreeAggregator $(CS407DIR)/Algorithms/NeighbourAggregate \
$(CS407DIR)/Algorithms/PredEval
PROJECT_SOURCEFILES = 

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = debug-helper.c sensor-converter.c predlang.c array-list.c map.c \
unique-array.c random-range.c tree-aggregator.c led-helper.c neighbour-detect.c \
neighbour-aggregate.c rimeaddr-helpers.c multipacket.c linked-list.c predicate-manager.c hop-data-manager.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code -Wmissing-field-initializers \
-Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wno-unused-parameter \
-Wuninitialized -Wold-style-definition -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type

CFLAGS += -Werror=bad-function-cast -Werror=implicit-function-declaration -Werror=format

#CFLAGS += -DNDEBUG
CFLAGS += -DNDEBUGVM

# Disable uip
#CONTIKI_NO_NET=1

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include

