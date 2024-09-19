
ifeq ($(CROSS_COMPILE),)
	CROSS_COMPILE =
else
	CROSS_COMPILE = aarch64-none-linux-gnu-
endif

CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall

WRITER_OBJ = writer.o

all: writer

writer: $(WRITER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f writer $(WRITER_OBJ)