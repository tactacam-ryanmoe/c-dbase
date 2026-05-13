CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -g
TARGET  = c-dbase
SRCS    = src/kvstore.c src/main.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c src/kvstore.h src/memtrack.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
