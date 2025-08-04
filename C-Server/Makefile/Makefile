CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99
TARGET=server

all: $(TARGET)

$(TARGET): server.c
	$(CC) $(CFLAGS) -o $(TARGET) server.c

run: all
	./server 8080

clean:
	rm -f $(TARGET)