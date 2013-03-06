CONTIKI_PROJECT = hsend
all: $(CONTIKI_PROJECT)

PROJECTDIRS := $(CS407DIR)/Algorithms/Common $(CS407DIR)/Algorithms/Common/containers $(CS407DIR)/PredicateLanguage
PROJECT_SOURCEFILES = 

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = debug-helper.c sensor-converter.c nhopreq.c predlang.c array-list.c map.c unique-array.c random-range.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code -Wmissing-field-initializers \
-Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wno-unused-parameter \
-Wuninitialized -Wold-style-definition -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type

CFLAGS += -Werror=bad-function-cast

CFLAGS += -DNDEBUG

# Disable uip
#CONTIKI_NO_NET=1

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include

