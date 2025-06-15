#include "http_response.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void http_response_init(http_response_t *res, int code, const char *msg) {
    res->status_code = code;
    res->status_msg = msg;
    res->content_type = "text/plain";
    res->body = NULL;
    res->body_length = 0;
}

void http_response_set_header(http_response_t *res, const char *type_key, const char *type_val) {
    if (strcmp(type_key, "Content-Type") == 0)
        res->content_type = type_val;
}

void http_response_set_body(http_response_t *res, const char *body, int len) {
    res->body = body;
    res->body_length = len;
}

void http_response_write(int fd, const http_response_t *res) {
    char header[256];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n\r\n",
        res->status_code, res->status_msg,
        res->content_type, res->body_length);

    write(fd, header, hlen);
    write(fd, res->body, res->body_length);
}
