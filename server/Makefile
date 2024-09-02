CC := $(shell if which ${CROSS_COMPILE} > /dev/null 2>&1; then echo "${CROSS_COMPILE}"; else echo "${CROSS_COMPILE}gcc"; fi )
CFLAGS := -g -Wall
OBJ := aesdsocket
# Target rule
all: ${OBJ}

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f aesdsocket

.PHONY: all clean

