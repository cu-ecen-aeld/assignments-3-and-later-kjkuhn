CC = $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall
LDFLAGS ?= -pthread


AESD_SOURCES = aesdsocket.c

.phony: all
all: aesdsocket

aesdsocket: $(AESD_SOURCES:.c=.o)
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -g -c -o $@ $<


.phony: clean
clean:
	rm -f aesdsocket $(AESD_SOURCES:.c=.o)

.phony: rebuild
rebuild: clean all