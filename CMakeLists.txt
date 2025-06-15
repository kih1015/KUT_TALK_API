cmake_minimum_required(VERSION 3.10)
project(KUT_TALK_API C)

# cJSON 라이브러리 수동 검색
find_path(CJSON_INCLUDE_DIR NAMES cJSON.h PATH_SUFFIXES cjson)
find_library(CJSON_LIBRARY NAMES cjson)

if (NOT CJSON_INCLUDE_DIR OR NOT CJSON_LIBRARY)
    message(FATAL_ERROR "cJSON library not found. Please install libcjson-dev or build cJSON from source.")
endif ()

find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mysql)
find_library(MYSQL_CLIENT_LIB NAMES mysqlclient)
if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_CLIENT_LIB)
    message(FATAL_ERROR "MySQL client library not found")
endif ()

# 헤더 포함 경로
include_directories(
        src
        ${CJSON_INCLUDE_DIR}
        ${MYSQL_INCLUDE_DIR}
)

# 소스 파일
set(SOURCES
        src/main.c
        src/controller/register_user_controller.c
        src/service/register_user_service.c
        src/adapter/register_user_adapter.c
        src/http_server/http_server.c
        src/http_server/handler_mapping.c
        src/http_server/handler_mapping.h
        src/repository/user_repository.c
        src/repository/user_repository.h
)

# 실행 파일 생성
add_executable(kut_talk_api ${SOURCES})

# cJSON 라이브러리 연결
target_link_libraries(kut_talk_api PRIVATE ${CJSON_LIBRARY} ${MYSQL_CLIENT_LIB})

# 컴파일 옵션 설정
if (MSVC)
    target_compile_options(kut_talk_api PRIVATE /W4)
else ()
    target_compile_options(kut_talk_api PRIVATE -Wall -Wextra -pedantic)
endif ()
