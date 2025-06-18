#pragma once
#include <stddef.h>

#include "http_server/http_request.h"

int user_controller_register(const struct http_request *, char *, size_t);
int user_controller_login   (const struct http_request *, char *, size_t);
int user_controller_get_me  (const struct http_request *, char *, size_t);
int user_controller_logout  (const struct http_request *, char *, size_t);
