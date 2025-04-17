CC ?= $(shell if which ${CROSS_COMPILE} > /dev/null 2>&1; then echo "${CROSS_COMPILE}"; else echo "${CROSS_COMPILE}gcc"; fi )
CFLAGS ?= -g -Wall
LDFLAGS ?= -lpthread
OBJS ?= aesdsocket
# Target rule
all: ${OBJS}

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f aesdsocket

.PHONY: all clean

