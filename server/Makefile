CC ?= $(shell if which ${CROSS_COMPILE} > /dev/null 2>&1; then echo "${CROSS_COMPILE}"; else echo "${CROSS_COMPILE}gcc"; fi )
CFLAGS ?= -g -Wall
LDFLAGS ?= -lpthread
TARGETS ?= aesdsocket
# Target rule
all: ${TARGETS}

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f aesdsocket

.PHONY: all clean

