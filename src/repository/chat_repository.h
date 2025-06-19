#pragma once
#include <stdint.h>

/* 채팅방 타입 */
enum room_type { ROOM_PRIVATE = 0, ROOM_PUBLIC = 1 };

/* 로비(내 채팅방) DTO */
struct lobby_room {
    uint32_t id;
    char title[81];
    char last_content[256];
    uint32_t last_sender;
    uint32_t last_time; /* epoch sec */
    uint32_t unread;
    uint32_t member_cnt;
};


/* public 방 DTO */
struct public_room {
    uint32_t id;
    char title[81];
    uint32_t member_cnt;
};

#define NICK_MAX  80
#define MSG_MAX  512

struct chat_msg_dto {
    uint32_t id; /* 메시지 PK           */
    uint32_t sender_id;
    char sender_nick[NICK_MAX + 1]; /* 보낸 사람 닉네임    */
    char content[MSG_MAX + 1];
    uint32_t created_at; /* epoch sec           */
    uint32_t unread_cnt; /* 아직 읽지 않은 인원 */
};

int chat_room_create(enum room_type type,
                     const char *title,
                     uint32_t creator_id,
                     const uint32_t *extra_members,
                     size_t mem_cnt,
                     uint32_t *out_room_id);

int chat_room_list_lobby(uint32_t me, struct lobby_room *out, size_t max, size_t *cnt);

int chat_room_list_public(struct public_room *out, size_t max, size_t *cnt);

int chat_room_join(uint32_t room, uint32_t user);

int chat_room_leave(uint32_t room, uint32_t user);

int chat_room_clear_unread(uint32_t user_id, uint32_t room_id);

int chat_message_list(
    uint32_t room_id,
    size_t page,
    size_t limit,
    struct chat_msg_dto *out,
    size_t *out_cnt
);

int chat_room_is_member(uint32_t room_id, uint32_t user_id);