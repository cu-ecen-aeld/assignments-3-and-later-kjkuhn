CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall
LDFLAGS ?= 
LIBS = -lrt -pthread

AESD_SOURCES = aesdsocket.c

.phony: all
all: aesdsocket

aesdsocket: $(AESD_SOURCES:.c=.o)
	$(CC) $(LDFLAGS) $^ $(LIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<


.phony: clean
clean:
	rm -f aesdsocket $(AESD_SOURCES:.c=.o)

.phony: rebuild
rebuild: clean all
