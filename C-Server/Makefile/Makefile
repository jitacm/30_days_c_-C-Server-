CC=gcc
CFLAGS=-Wall -Wextra -pedantic -std=c99 -D_GNU_SOURCE
LDFLAGS=-lpthread
TARGET=server
SRC=Http_server.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: all
	./$(TARGET) 8080

clean:
	rm -f $(TARGET) *.log

test: all
	@echo "Starting server for testing..."
	@./$(TARGET) 8080 &
	@sleep 2
	@echo "\nTesting GET /"
	@curl -i http://localhost:8080/
	@echo "\n\nTesting GET /time"
	@curl -i http://localhost:8080/time
	@echo "\n\nTesting GET /status"
	@curl -i http://localhost:8080/status
	@echo "\n\nTesting HEAD /"
	@curl -I http://localhost:8080/
	@echo "\nKilling test server..."
	@pkill -f "./$(TARGET)"

.PHONY: all run clean test
