#include "repository/user_repository.h"
#include <stdio.h>
#include <string.h>

#include "util/password_hash.h"

MYSQL *db_conn = NULL;

int user_repository_init(
    const char *host,
    const char *user,
    const char *pass,
    const char *db,
    unsigned int port
) {
    db_conn = mysql_init(NULL);
    if (!db_conn) return -1;
    if (!mysql_real_connect(db_conn, host, user, pass, db, port,NULL, 0)) {
        fprintf(stderr, "MySQL connect error: %s\n", mysql_error(db_conn));
        mysql_close(db_conn);
        db_conn = NULL;
        return -2;
    }
    return 0;
}

void user_repository_close(void) {
    if (db_conn) {
        mysql_close(db_conn);
        db_conn = NULL;
    }
}

int user_repository_exists(const char *userid) {
    if (!db_conn) return -1;
    MYSQL_STMT *stmt = mysql_stmt_init(db_conn);
    const char *sql =
            "SELECT COUNT(*) FROM users WHERE userid = ?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        mysql_stmt_close(stmt);
        return -2;
    }
    MYSQL_BIND bind = {0};
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (char *) userid;
    bind.buffer_length = strlen(userid);
    mysql_stmt_bind_param(stmt, &bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return -3;
    }
    MYSQL_BIND result = {0};
    my_ulonglong count = 0;
    result.buffer_type = MYSQL_TYPE_LONGLONG;
    result.buffer = (char *) &count;
    result.buffer_length = sizeof(count);
    mysql_stmt_bind_result(stmt, &result);
    mysql_stmt_store_result(stmt);
    mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);
    return count > 0 ? 1 : 0;
}

int user_repository_nickname_exists(const char *nickname) {
    if (!db_conn) return -1;
    MYSQL_STMT *stmt = mysql_stmt_init(db_conn);
    const char *sql = "SELECT COUNT(*) FROM users WHERE nickname = ?";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        mysql_stmt_close(stmt);
        return -2;
    }
    MYSQL_BIND bind = {0};
    bind.buffer_type = MYSQL_TYPE_STRING;
    bind.buffer = (char *) nickname;
    bind.buffer_length = strlen(nickname);
    mysql_stmt_bind_param(stmt, &bind);
    if (mysql_stmt_execute(stmt)) {
        mysql_stmt_close(stmt);
        return -3;
    }
    MYSQL_BIND result = {0};
    my_ulonglong count = 0;
    result.buffer_type = MYSQL_TYPE_LONGLONG;
    result.buffer = &count;
    result.buffer_length = sizeof(count);
    mysql_stmt_bind_result(stmt, &result);
    mysql_stmt_store_result(stmt);
    mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);
    return count > 0 ? 1 : 0;
}

int user_repository_add(
    const char *userid,
    const char *nickname,
    const char *password_plain   /* 평문 전달 */
){
    if (!db_conn) return -1;

    /* 1) 평문 → Argon2id 해시 ---------------------------------------- */
    char hash[PWD_HASH_LEN];
    if (kta_password_hash(password_plain, hash) != 0)
        return -99;                     /* 해시 실패: 적당한 오류 코드 */

    MYSQL_STMT *stmt = mysql_stmt_init(db_conn);
    const char *sql =
        "INSERT INTO users (userid, nickname, password) VALUES (?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        int err = mysql_errno(db_conn);
        mysql_stmt_close(stmt);
        return err;
    }

    MYSQL_BIND bind[3] = {0};

    bind[0].buffer_type   = MYSQL_TYPE_STRING;
    bind[0].buffer        = (char *)userid;
    bind[0].buffer_length = strlen(userid);

    bind[1].buffer_type   = MYSQL_TYPE_STRING;
    bind[1].buffer        = (char *)nickname;
    bind[1].buffer_length = strlen(nickname);

    bind[2].buffer_type   = MYSQL_TYPE_STRING;
    bind[2].buffer        = hash;                 /* 해시 문자열 저장 */
    bind[2].buffer_length = strlen(hash);

    mysql_stmt_bind_param(stmt, bind);
    if (mysql_stmt_execute(stmt)) {
        int err = mysql_errno(db_conn);
        mysql_stmt_close(stmt);
        return err;
    }
    mysql_stmt_close(stmt);
    return 0;
}
