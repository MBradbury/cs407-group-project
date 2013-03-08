CONTIKI_PROJECT = distance-measure
all: $(CONTIKI_PROJECT)

TARGET = sky

CFLAGS = -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes \
-Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wbad-function-cast \
-Wnested-externs -Wwrite-strings -Wconversion -Wunreachable-code -Wmissing-field-initializers \
-Wno-int-to-pointer-cast -Winit-self -Wstrict-aliasing -Wno-unused-parameter \
-Wuninitialized -Wold-style-definition -Wredundant-decls -Werror-implicit-function-declaration \
-Wunused -Wno-unused-value -Wreturn-type

CFLAGS += -Werror=bad-function-cast -Werror=implicit-function-declaration -Werror=format

CONTIKI = $(HOME)/contiki-2.6
include $(CONTIKI)/Makefile.include
