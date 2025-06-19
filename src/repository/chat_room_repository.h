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
};

/* public 방 DTO */
struct public_room {
    uint32_t id;
    char title[81];
    uint32_t member_cnt;
};
