
CFLAGS=-Wall -g -pedantic

SYSOBJ=linux.o
IPFT_OBJS=ipft.o util.o $(SYSOBJ)
IPFT_HEADS=ipft.h

.PHONY: default clean

default: ipft

clean:
	$(RM) $(IPFT_OBJS)


ipft: $(IPFT_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(IPFT_OBJS): $(IPFT_HEADS)
