#native compiler
NC=gcc
#cross compiler
CC=aarch64-none-linux-gnu-
CFLAGS= -Wall -g
TARGET=writer
SRC=writer.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	${CC}$(NC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o
