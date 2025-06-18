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

int user_adapter_get_me(
    const char *raw_req,
    char *resp_buf, size_t buf_sz
);

int user_adapter_logout(
    const char *raw_req,
    char *resp_buf, size_t buf_sz
);
