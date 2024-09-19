
ifdef CROSS_COMPILE
	CC_PREFIX = aarch64-none-linux-gnu-
else
	CC_PREFIX =
endif

CC = $(CC_PREFIX)gcc
CFLAGS = -Wall

WRITER_OBJ = writer.o

all: writer

writer: $(WRITER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f writer $(WRITER_OBJ)