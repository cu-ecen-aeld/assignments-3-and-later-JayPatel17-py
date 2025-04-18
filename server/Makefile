# Ensure CROSS_COMPILE is defined for cross-compiling
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall
LDFLAGS ?= -lpthread
TARGET ?= aesdsocket
OBJS ?= aesdsocket.o

# Default target
all: $(TARGET)

# Build the target executable
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile each .c file into .o object file
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean up build artifacts
clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
