#pragma once

#include <stddef.h>

int user_adapter_register(
    const char *body,
    char *resp_buf,
    size_t buf_size
);

int user_adapter_login(
    const char *body,
    char *resp_buf,
    size_t buf_sz
);
