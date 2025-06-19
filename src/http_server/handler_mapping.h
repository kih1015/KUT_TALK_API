#pragma once
#include <regex.h>

#include "controller/user_controller.h"

void *handle_client_thread(void *arg);

typedef int (*controller_cb)(const struct http_request *, char *, size_t);

struct route {
    const char *method;
    const char *pattern; /* 고정 경로 or 정규식 */
    controller_cb handler;
    int is_regex; /* 0 = strcmp, 1 = regex */
    regex_t re; /* is_regex==1 일 때만 사용 */
};
