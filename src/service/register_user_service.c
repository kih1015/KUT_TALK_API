#include "service/register_user_service.h"
#include <string.h>

#define MAX_USERS 100
#define ID_LEN      64
#define NICK_LEN    64
#define PW_LEN      64

// 내부 저장용 구조체
typedef struct {
    char userid[ID_LEN];
    char nickname[NICK_LEN];
    char password[PW_LEN];
} user_t;

static user_t users[MAX_USERS];
static int   user_count = 0;

int register_user_service(
    const char *userid,
    const char *nickname,
    const char *password
) {
    // 1) 중복 확인
    for (int i = 0; i < user_count; ++i) {
        if (strcmp(users[i].userid, userid) == 0) {
            return -1;  // 중복된 ID
        }
    }

    // 2) 공간 확인
    if (user_count >= MAX_USERS) {
        return -2;  // 저장 공간 부족
    }

    // 3) 데이터 복사
    strncpy(users[user_count].userid,   userid,   ID_LEN - 1);
    users[user_count].userid[ID_LEN-1] = '\0';

    strncpy(users[user_count].nickname, nickname, NICK_LEN - 1);
    users[user_count].nickname[NICK_LEN-1] = '\0';

    strncpy(users[user_count].password, password, PW_LEN - 1);
    users[user_count].password[PW_LEN-1] = '\0';

    user_count++;
    return 0;
}
