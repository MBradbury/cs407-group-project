CONTIKI_PROJECT = hsend
all: $(CONTIKI_PROJECT)

PROJECTDIRS := $(CS407DIR)/Algorithms/Common $(CS407DIR)/Algorithms/NHopReq $(CS407DIR)/PredicateLanguage
PROJECT_SOURCEFILES = 

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = debug-helper.c sensor-converter.c nhopreq.c predlang.c linked-list.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code \
-Wno-unused-parameter -Wuninitialized -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type\
-DNDEBUG

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include

