# Flags
CC=gcc
CFLAGS = -lrt -lm -w -lmy_lib -pthread

# Paths
BIN_DIR=bin
SRC_DIR=.

# Arguments
# 192.168.18.68
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
	sudo $(BIN_DIR)/server $(PORT)

.PHONY: clean

clean:
	rm -rf $(BIN_DIR)