#pragma once
#include <stddef.h>

/* body(POST JSON) → HTTP 응답 직렬화 길이(−1 = buf 부족) */
int user_controller_register(const char *body,
                             char *resp_buf, size_t buf_sz);

int user_controller_login(const char *body,
                          char *resp_buf, size_t buf_sz);

/* raw_req = 전체 HTTP 요청 헤더·바디 문자열 */
int user_controller_get_me(const char *raw_req,
                           char *resp_buf, size_t buf_sz);

int user_controller_logout(const char *raw_req,
                           char *resp_buf, size_t buf_sz);
