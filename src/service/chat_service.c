#include "chat_service.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "repository/session_repository.h"
#include "repository/chat_repository.h"

/* ──────────────────────────────────────────────────────── */
/* 세션 → user_id (INT)                                     */
/* ──────────────────────────────────────────────────────── */
static int uid_from_session(const char *sid, uint32_t *out_uid) {
    if (!sid || !*sid) return -1;

    time_t exp;
    uint32_t uid;

    /* sessions.id ⇒ users.id */
    if (session_repository_find_id(sid, &uid, &exp) != 0) return -1;
    if (exp < time(NULL)) return -1;

    *out_uid = uid;
    return 0;
}

/* ──────────────────────────────────────────────────────── */
/* 채팅방 생성                                             */
/* ──────────────────────────────────────────────────────── */
int chat_service_create_room(const char *sid,
                             enum room_type type,
                             const char *title,
                             const uint32_t *member_ids, size_t mcnt,
                             uint32_t *room_id) {
    uint32_t me;
    if (uid_from_session(sid, &me) != 0) return CHAT_ERR_BAD_SESSION;

    if (!title || !*title) return -3;
    if (mcnt > 50) mcnt = 50;

    return chat_room_create(type, title, me,
                            member_ids, mcnt, room_id);
}

/* ──────────────────────────────────────────────────────── */
/* 내 로비 목록                                            */
/* ──────────────────────────────────────────────────────── */
int chat_service_my_rooms(const char *sid,
                          struct lobby_room *out, size_t max, size_t *cnt) {
    uint32_t me;
    if (uid_from_session(sid, &me) != 0) return CHAT_ERR_BAD_SESSION;

    return chat_room_list_lobby(me, out, max, cnt);
}

/* ──────────────────────────────────────────────────────── */
/* Public 방 목록                                          */
/* ──────────────────────────────────────────────────────── */
int chat_service_list_public(struct public_room *out, size_t max, size_t *cnt) {
    return chat_room_list_public(out, max, cnt);
}

/* ──────────────────────────────────────────────────────── */
/* 방 참가                                                 */
/* ──────────────────────────────────────────────────────── */
int chat_service_join(const char *sid, uint32_t room_id) {
    uint32_t me;
    if (uid_from_session(sid, &me) != 0) return CHAT_ERR_BAD_SESSION;
    return chat_room_join(room_id, me); /* 0 / -1(이미) / -2 */
}

/* ──────────────────────────────────────────────────────── */
/* 방 탈퇴                                                 */
/* ──────────────────────────────────────────────────────── */
int chat_service_leave(const char *sid, uint32_t room_id) {
    uint32_t me;
    if (uid_from_session(sid, &me) != 0) return CHAT_ERR_BAD_SESSION;

    int rc = chat_room_leave(room_id, me); /* 0 / -1 / -2 */
    if (rc != 0) return rc;

    chat_room_clear_unread(me, room_id); /* 미읽음 정리 */
    return 0;
}

int chat_service_get_messages(
    const char *sid,
    uint32_t room_id,
    size_t page,
    size_t limit,
    struct chat_msg_dto *out,
    size_t *out_cnt
) {
    uint32_t me;
    if (uid_from_session(sid, &me) != 0)
        return CHAT_ERR_BAD_SESSION;

    /* 방 멤버인지 확인 (0=멤버, 1=아님, -1=DB 오류) */
    int rc = chat_room_is_member(room_id, me);
    if (rc == 1) return CHAT_ERR_BAD_SESSION;
    if (rc != 0) return -1;

    if (limit == 0) limit = 200; /* 기본·최대값 정의 */

    return chat_message_list(room_id, page, limit, out, out_cnt);
}
