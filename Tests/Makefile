CC=gcc
CFLAGS= -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wfloat-equal
CFLAGS += -Os -fno-strict-aliasing -ffunction-sections
CFLAGS += -I. -I../Common/

ODIR=obj
LDIR=lib

LDFLAGS= -B -Felf

_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o ../Common/array-list.o ../Common/linked-list.o ../Common/unique-array.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

all: directories build

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

build: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean directories

directories:
	rm -rf $(ODIR) $(LDIR)
	mkdir $(ODIR) $(LDIR)

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
