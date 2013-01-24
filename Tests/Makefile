CC=gcc
CFLAGS= -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wfloat-equal
CFLAGS += -g
CFLAGS += -I. -I../Algorithms/Common/

ODIR=obj
LDIR=lib

LDFLAGS= -B -Felf

_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = main.o ../Algorithms/Common/array-list.o ../Algorithms/Common/linked-list.o ../Algorithms/Common/unique-array.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

all: directories tests

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $(patsubst %,$(ODIR)/%,$(notdir $@)) $< $(CFLAGS)

tests: $(OBJ)
	$(CC) -o $@ $(patsubst %,$(ODIR)/%,$(notdir $^)) $(CFLAGS) $(LDFLAGS)

.PHONY: clean directories

directories:
	rm -rf $(ODIR) $(LDIR)
	mkdir $(ODIR) $(LDIR)

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~

