#pragma once

#include <mysql/mysql.h>

void user_repository_close(void);

int user_repository_exists(const char *userid);

int user_repository_nickname_exists(const char *nickname);

int user_repository_add(
    const char *userid,
    const char *nickname,
    const char *password
);

int user_repository_get_hash(
    const char *userid,
    char *out_hash,
    size_t buf_size)
;

struct user_info {
    char userid[256];
    char nickname[256];
};

int user_repository_get_info(const char *userid, struct user_info *out);

#define USERID_MAX   255
#define NICK_MAX     255

int user_repository_get_info_by_id(uint32_t id,
                                   struct user_info *out);

int user_repository_get_id_by_userid(const char *uid, uint32_t *out_id);
