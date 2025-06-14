CC := gcc
CFLAGS := -Wall -Wextra -g \
          -Isrc \
          -Isrc/http_server \
          -Isrc/user \
          -Isrc/user/controller \
          -Isrc/user/service
LDFLAGS := -lpthread -lcjson

SRC := \
    main.c \
    src/http_server/http_server.c \
    src/user/controller/register_user_controller.c \
    src/user/service/register_user_service.c

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
