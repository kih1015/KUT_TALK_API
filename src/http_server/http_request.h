#pragma once
#include <stddef.h>

#define MAX_METHOD_LEN   8
#define MAX_PATH_LEN   256
#define MAX_HEADERS     32

struct http_header {
    char name[32];
    char value[256];
};

struct http_request {
    const char *raw;

    char method[MAX_METHOD_LEN];
    char path[MAX_PATH_LEN];

    struct http_header headers[MAX_HEADERS];
    size_t header_cnt;

    const char *body;
};

int parse_http_request(char *buf, size_t len, struct http_request *out);
const char *http_get_header(const struct http_request *req, const char *name);