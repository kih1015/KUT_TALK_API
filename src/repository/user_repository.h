#pragma once

#include <mysql/mysql.h>

extern MYSQL *db_conn;

int user_repository_init(
    const char *host, const char *user,
    const char *pass, const char *db, unsigned int port
);

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
