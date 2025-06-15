#include <stdio.h>

#include "http_server/http_server.h"
#include "repository/user_repository.h"

int main(void) {
    const char *db_user = getenv("DB_USER");
    const char *db_pass = getenv("DB_PASS");

    if (user_repository_init(
            "127.0.0.1", db_user, db_pass,
            "kuttalk_db", 3306) != 0) {
        fprintf(stderr, "DB init failed\n");
        return 1;
    }

    http_server_start(8080);

    user_repository_close();
    return 0;
}
