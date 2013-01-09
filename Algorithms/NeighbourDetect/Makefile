CONTIKI_PROJECT = neighbour-detect
all: $(CONTIKI_PROJECT)

# Target references:
# http://www.sics.se/contiki/wiki/index.php/Contiki_make_targets

TARGET = sky

PROJECTDIRS := $(CS407DIR)/Algorithms/Common
PROJECT_SOURCEFILES =

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = sensor-converter.c debug-helper.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code -Wmissing-field-initializers \
-Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wno-unused-parameter \
-Wuninitialized -Wold-style-definition -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include
