CONTIKI_PROJECT = hcluster
all: $(CONTIKI_PROJECT)

# Target references:
# http://www.sics.se/contiki/wiki/index.php/Contiki_make_targets

TARGET = sky

PROJECTDIRS := $(CS407DIR)/Algorithms/Common
PROJECT_SOURCEFILES = 

CONTIKIDIRS :=
CONTIKI_SOURCEFILES = sensor-converter.c debug-helper.c logging.c

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code \
-Wno-unused-parameter -Wuninitialized -Wredundant-decls \
-Werror-implicit-function-declaration -Wunused -Wno-unused-value -Wreturn-type \
#-Wmissing-field-initializers -Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wold-style-definition 

CFLAGS += -Werror=bad-function-cast -Werror=format

CONTIKI = $(CS407DIR)/contiki
include $(CONTIKI)/Makefile.include
