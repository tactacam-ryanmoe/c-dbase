CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -pedantic -g
TARGET  = c-dbase
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

ifeq ($(OBJS),)
$(TARGET):
else
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^
endif

%.o: %.c src/kvstore.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
