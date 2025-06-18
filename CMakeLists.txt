cmake_minimum_required(VERSION 3.16)
project(KUT_TALK_API C)

# -------- 공통 컴파일 옵션 / 표준 ------------------------------------------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# -------- 외부 라이브러리 ---------------------------------------------------

# 1) cJSON
find_path(CJSON_INCLUDE_DIR   NAMES cJSON.h        PATH_SUFFIXES cjson)
find_library(CJSON_LIBRARY    NAMES cjson)
if (NOT CJSON_INCLUDE_DIR OR NOT CJSON_LIBRARY)
    message(FATAL_ERROR "cJSON library not found. sudo apt install libcjson-dev")
endif()

# 2) MySQL client
find_path(MYSQL_INCLUDE_DIR   NAMES mysql.h        PATH_SUFFIXES mysql)
find_library(MYSQL_CLIENT_LIB NAMES mysqlclient)
if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_CLIENT_LIB)
    message(FATAL_ERROR "MySQL client library not found. sudo apt install libmysqlclient-dev")
endif()

# 3) libsodium (Argon2id)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium>=1.0.18)

# -------- 빌드 타깃 ---------------------------------------------------------

file(GLOB SRC
        src/*.c
        src/*/*.c)

add_executable(kut_talk_api ${SRC}
        src/util/session_id.c
        src/util/session_id.h
        src/repository/session_repository.c
        src/repository/session_repository.h
        src/http_server/http_status.h
        src/http_server/http_utils.h
        src/http_server/http_request.h
        src/http_server/http_request.c)

target_include_directories(kut_talk_api PRIVATE
        src
        ${CJSON_INCLUDE_DIR}
        ${MYSQL_INCLUDE_DIR}
        ${SODIUM_INCLUDE_DIRS})

target_link_libraries(kut_talk_api PRIVATE
        ${CJSON_LIBRARY}
        ${MYSQL_CLIENT_LIB}
        ${SODIUM_LIBRARIES})
