#include "register_user_service.h"
#include <string.h>

#define MAX_USERS 100

static register_user_dto_t users[MAX_USERS];
static int user_count = 0;

int register_user_service(const register_user_dto_t *dto) {
    // 중복 확인
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].userid, dto->userid) == 0) {
            return -1; // 중복된 ID
        }
    }

    if (user_count >= MAX_USERS) {
        return -2; // 저장 공간 부족
    }

    users[user_count++] = *dto;
    return 0;
}
