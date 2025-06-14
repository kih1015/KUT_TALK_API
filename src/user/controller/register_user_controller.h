#ifndef REGISTER_USER_CONTROLLER_H
#define REGISTER_USER_CONTROLLER_H

#include "http_response.h"

int register_user_controller(const char *body, http_response_t *out_res);

#endif
