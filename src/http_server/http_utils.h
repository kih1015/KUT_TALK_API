#pragma once
#include "http_status.h"
#include <stdio.h>
#include <string.h>

static inline int
build_simple_resp(char *resp_buf,
                  size_t buf_sz,
                  enum http_status status,
                  const char *body) {
    const char *reason = http_reason_phrase(status);
    int body_len = body ? (int) strlen(body) : 0;

    int n = snprintf(resp_buf, buf_sz,
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "\r\n"
                     "%s",
                     status, reason, body_len,
                     body ? body : "");

    return (n < 0 || (size_t) n >= buf_sz) ? -1 : n;
}
