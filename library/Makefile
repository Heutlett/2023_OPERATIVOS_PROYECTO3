CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
AR = ar rcs

SRCS = my_lib.c
OBJS = $(SRCS:.c=.o)
LIBRARY = libmy_lib.a

.PHONY: all clean

all: $(LIBRARY)
	sudo cp my_lib.h /usr/include
	sudo mv libmy_lib.a /usr/lib

$(LIBRARY): $(OBJS)
	$(AR) $@ $(OBJS)

bin/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIBRARY)
	sudo rm /usr/include/my_lib.h
	sudo rm /usr/lib/libmy_lib.a

