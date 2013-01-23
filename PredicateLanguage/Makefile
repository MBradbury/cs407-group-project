CC=gcc
CFLAGS= -Wall -W -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -Wfloat-equal
CFLAGS += -Os -fno-strict-aliasing -ffunction-sections
CFLAGS += -I.
CFLAGS += -DMAIN_FUNC -DENABLE_CODE_GEN -DNDEBUG

ODIR=obj
LDIR=lib

LDFLAGS= -B -Felf -lm

_DEPS = 
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = predlang.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

all: directories predlang

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

predlang: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean directories

directories:
	rm -rf $(ODIR) $(LDIR)
	mkdir $(ODIR) $(LDIR)

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~

