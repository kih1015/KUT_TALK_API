#pragma once
#include <stddef.h>

#include "controller/user_controller.h"

void *handle_client_thread(void *arg);

typedef int (*controller_cb)(
    const struct http_request *req,
    char *resp_buf, size_t resp_sz);

struct route {
    const char *method, *path;
    controller_cb handler;
};

extern const struct route ROUTES[];
extern const size_t ROUTE_CNT;
