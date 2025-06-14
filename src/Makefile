# src/Makefile

CC := gcc
CFLAGS := -Wall -O2 -Ihttp_server
LDFLAGS :=

SRC := main.c \
       http_server/http_server.c
OBJ := $(SRC:.c=.o)
TARGET := http_app

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
