# Flags
CC=gcc
CFLAGS = -lmy_lib

# Paths
BIN_DIR=bin
SRC_DIR=.

all: test

bin:
	mkdir -p $(BIN_DIR)

test: bin
	$(CC) $(SRC_DIR)/main.c $(CFLAGS) -o $(BIN_DIR)/main

clean:
	rm -rf $(BIN_DIR)
