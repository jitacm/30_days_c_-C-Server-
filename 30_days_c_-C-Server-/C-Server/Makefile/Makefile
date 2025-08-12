CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99
TARGET=server
SRC=Http_server.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: all
	./$(TARGET) 8080

clean:
	rm -f $(TARGET)
