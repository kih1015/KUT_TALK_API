# 컴파일러 설정
CC := gcc
CFLAGS := -Wall -Wextra -g \
          -Isrc \
          -Isrc/http_server \
          -Isrc/user/controller \
          -Isrc/user/service
LDFLAGS := -lpthread -lcjson

# 소스 파일 목록
SRC := \
    src/main.c \
    src/http_server/http_server.c \
    src/user/controller/register_user_controller.c \
    src/user/service/register_user_service.c

# 오브젝트 파일 자동 추출
OBJ := $(SRC:.c=.o)

# 실행 파일 이름
TARGET := http_app

# 기본 목표
all: $(TARGET)

# 링크
$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# 개별 컴파일
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 정리
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
