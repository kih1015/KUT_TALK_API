#pragma once
#include <stddef.h>
#include "repository/chat_repository.h"     /* DTO 구조체 */

#define CHAT_ERR_BAD_SESSION   -401   /* 세션 없음/만료 */

int chat_service_create_room(
    const char *session_id,
    enum room_type type,
    const char *title,
    const uint32_t *member_ids, size_t member_cnt,
    uint32_t *out_room_id);

int chat_service_my_rooms(
    const char *session_id,
    struct lobby_room *out, size_t max, size_t *out_cnt);

int chat_service_list_public(
    struct public_room *out, size_t max, size_t *out_cnt);

int chat_service_join(
    const char *session_id, uint32_t room_id);

int chat_service_leave(
    const char *session_id, uint32_t room_id);

int user_repository_get_id_by_userid(const char *userid, uint32_t *out_id);
