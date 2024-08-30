CC := $(shell if which ${CROSS_COMPILE} > /dev/null 2>&1; then echo "${CROSS_COMPILE}"; else echo "${CROSS_COMPILE}gcc"; fi )
CFLAGS := -g -Wall
OBJ := aesdsocket.o server.o client.o
# Target rule
all: ${OBJ}

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f *.o

.PHONY: all clean

