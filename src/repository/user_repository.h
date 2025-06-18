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
