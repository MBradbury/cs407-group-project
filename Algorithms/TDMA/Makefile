CONTIKI_PROJECT = tdma
all: $(CONTIKI_PROJECT)

# Valid TYPEs are PELP PELE PEGP PEGE
PRED_TYPE = PELE

ifeq ($(PRED_TYPE),PELP)
	PREDICATE_TYPE_DIR = PredEvalLocalPeriodic
	PREDICATE_TYPE_FILE = pelp.c
endif
ifeq ($(PRED_TYPE),PELE)
	PREDICATE_TYPE_DIR = PredEvalLocalEvent
	PREDICATE_TYPE_FILE = pele.c
endif
ifeq ($(PRED_TYPE),PEGP)
	PREDICATE_TYPE_DIR = PredEvalGlobalPeriodic
	PREDICATE_TYPE_FILE = pegp.c
endif
ifeq ($(PRED_TYPE),PEGE)
	PREDICATE_TYPE_DIR = PredEvalGlobalEvent
	PREDICATE_TYPE_FILE = pege.c
endif


PROJECTDIRS := $(CS407DIR)/Algorithms/Common $(CS407DIR)/Algorithms/Common/containers \
$(CS407DIR)/Algorithms/Common/net $(CS407DIR)/PredicateLanguage \
$(CS407DIR)/Algorithms/TreeAggregator $(CS407DIR)/Algorithms/NeighbourAggregate \
$(CS407DIR)/Algorithms/PredEval $(CS407DIR)/Algorithms/$(PREDICATE_TYPE_DIR)
PROJECT_SOURCEFILES := 

CONTIKIDIRS :=
CONTIKI_SOURCEFILES := debug-helper.c sensor-converter.c predlang.c array-list.c map.c \
unique-array.c random-range.c tree-aggregator.c led-helper.c neighbour-detect.c \
neighbour-aggregate.c rimeaddr-helpers.c multipacket.c linked-list.c predicate-manager.c hop-data-manager.c \
eventupdate.c nhopflood.c $(PREDICATE_TYPE_FILE) nhopreq.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code -Wmissing-field-initializers \
-Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wno-unused-parameter \
-Wuninitialized -Wold-style-definition -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type -Winline

CFLAGS += -Werror=bad-function-cast -Werror=implicit-function-declaration -Werror=format

#CFLAGS += -DNDEBUG
CFLAGS += -DNDEBUGVM -DRIMESTATS_CONF_ENABLED=1 -DENERGEST_CONF_ON=1 -DPREDICATE_TYPE_$(PRED_TYPE)=1

CFLAGS += -fdata-sections -DPROCESS_CONF_NO_PROCESS_NAMES=1

CONTIKI = $(CS407DIR)/contiki
include $(CONTIKI)/Makefile.include
