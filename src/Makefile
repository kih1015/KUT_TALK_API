# Makefile 위치: src/
CC := gcc
CFLAGS := -Wall -Wextra -g -I.
LDFLAGS := -lpthread -lcjson

SRC := \
    main.c \
    http_server/http_server.c \
    user/controller/register_user_controller.c \
    user/service/register_user_service.c

OBJ := $(SRC:.c=.o)
TARGET := http_app

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
