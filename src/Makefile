# 컴파일러 및 플래그 설정
CC := gcc
CFLAGS := -Wall -Wextra -g \
          -I. \
          -Isrc \
          -Isrc/http_server \
          -Isrc/user \
          -Isrc/user/controller \
          -Isrc/user/service

LDFLAGS := -lpthread -lcjson

# 소스 파일 목록 (DTO는 헤더만 존재하므로 제외)
SRC := \
    main.c \
    src/http_server/http_server.c \
    src/user/controller/register_user_controller.c \
    src/user/service/register_user_service.c

# 오브젝트 및 타겟
OBJ := $(SRC:.c=.o)
TARGET := http_app

# 빌드 기본 목표
all: $(TARGET)

# 링크 단계
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# .c → .o 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 정리
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
