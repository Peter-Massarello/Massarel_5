CC = gcc
CFLAGS = -g -std=c99

OSS = oss.o
UPROC = uproc.o

all: oss uproc

oss: $(OSS)
	$(CC) -o $@ $(OSS) $(CFLAGS)

uproc: $(UPROC)
	$(CC) -o $@ $(UPROC) $(CFLAGS)

$(OSS): oss.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(UPROC): uproc.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f *.o *.txt *.log oss uproc