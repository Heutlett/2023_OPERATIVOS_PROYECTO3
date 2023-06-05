# Flags
CC=gcc
CFLAGS = -lrt -lm -w

# Paths
BIN_DIR=bin
SRC_DIR=.

# Arguments
IP ?= 127.0.0.1
PORT ?= 8080

all: server client

bin:
	mkdir -p $(BIN_DIR)

client: bin
ifeq ($(MAKECMDGOALS),client)
	$(CC) $(SRC_DIR)/client.c $(CFLAGS) -o $(BIN_DIR)/client
	$(BIN_DIR)/client $(IP) $(PORT)
endif

server: bin
	$(CC) $(SRC_DIR)/server.c $(CFLAGS) -o $(BIN_DIR)/server
	sudo $(BIN_DIR)/server $(IP) $(PORT)

.PHONY: clean

clean:
	rm -rf $(BIN_DIR)


.PHONY: check_port