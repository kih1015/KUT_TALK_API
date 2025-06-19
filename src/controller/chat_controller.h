#pragma once
#include "http_server/http_request.h"
#include <stddef.h>

/* 채팅방 생성 */
int chat_controller_create_room (const struct http_request *req,
                                 char *resp, size_t sz);
/* 내 로비 목록 */
int chat_controller_my_rooms    (const struct http_request *req,
                                 char *resp, size_t sz);
/* Public 방 목록 */
int chat_controller_list_public (const struct http_request *req,
                                 char *resp, size_t sz);
/* 방 참가 */
int chat_controller_join        (const struct http_request *req,
                                 char *resp, size_t sz);
/* 방 탈퇴 */
int chat_controller_leave       (const struct http_request *req,
                                 char *resp, size_t sz);
