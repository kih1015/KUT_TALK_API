#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

typedef struct {
    int status_code;
    const char *status_msg;
    const char *content_type;
    const char *body;
    int body_length;
} http_response_t;

void http_response_init(http_response_t *res, int code, const char *msg);
void http_response_set_header(http_response_t *res, const char *type_key, const char *type_val);
void http_response_set_body(http_response_t *res, const char *body, int len);
void http_response_write(int fd, const http_response_t *res);

#endif
