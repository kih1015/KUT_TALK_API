#include <stdio.h>
#include <sodium.h>

#include "http_server/http_server.h"
#include "repository/db.h"

int main(void) {
    setvbuf(stderr,NULL,_IOLBF, 0);
    const char *db_user = getenv("DB_USER");
    const char *db_pass = getenv("DB_PASS");

    if (sodium_init() < 0) {
        return 1;
    }

    if (db_global_init(
            "127.0.0.1", db_user, db_pass,
            "kuttalk_db", 3306) != 0) {
        fprintf(stderr, "DB init failed\n");
        return 1;
    }

    http_server_start(8080);

    db_global_end();
    return 0;
}
