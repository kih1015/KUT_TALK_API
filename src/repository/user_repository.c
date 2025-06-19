#include "repository/user_repository.h"
#include <stdio.h>
#include <string.h>

#include "util/password_hash.h"

#include "db.h"

void user_repository_close(void) {
    MYSQL *db_conn = get_db();
    if (db_conn) {
        mysql_close(db_conn);
        db_conn = NULL;
    }
}

int user_repository_exists(const char *userid) {
    MYSQL *db_conn = get_db();
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
    MYSQL *db_conn = get_db();
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
    const char *password_plain /* 평문 전달 */
) {
    MYSQL *db_conn = get_db();
    if (!db_conn) return -1;

    /* 1) 평문 → Argon2id 해시 ---------------------------------------- */
    char hash[PWD_HASH_LEN];
    if (kta_password_hash(password_plain, hash) != 0)
        return -99; /* 해시 실패: 적당한 오류 코드 */

    MYSQL_STMT *stmt = mysql_stmt_init(db_conn);
    const char *sql =
            "INSERT INTO users (userid, nickname, password) VALUES (?, ?, ?)";
    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        int err = mysql_errno(db_conn);
        mysql_stmt_close(stmt);
        return err;
    }

    MYSQL_BIND bind[3] = {0};

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char *) userid;
    bind[0].buffer_length = strlen(userid);

    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (char *) nickname;
    bind[1].buffer_length = strlen(nickname);

    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = hash; /* 해시 문자열 저장 */
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

int user_repository_get_hash(const char *userid, char *out_hash, size_t buf_size) {
    MYSQL *db_conn = get_db();
    if (!db_conn) return -1; /* DB 미초기화 */

    MYSQL_STMT *stmt = mysql_stmt_init(db_conn);
    const char *sql = "SELECT password FROM users WHERE userid = ?";

    if (mysql_stmt_prepare(stmt, sql, strlen(sql))) {
        mysql_stmt_close(stmt);
        return -2; /* prepare 실패 */
    }

    /* 파라미터 바인드 */
    MYSQL_BIND param = {0};
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = (char *) userid;
    param.buffer_length = strlen(userid);
    mysql_stmt_bind_param(stmt, &param);

    if (mysql_stmt_execute(stmt)) {
        /* 실행 */
        mysql_stmt_close(stmt);
        return -3; /* execute 오류 */
    }

    /* 결과 바인드 */
    memset(out_hash, 0, buf_size); /* 안전: 널 종료 */
    MYSQL_BIND result = {0};
    result.buffer_type = MYSQL_TYPE_STRING;
    result.buffer = out_hash;
    result.buffer_length = buf_size - 1; /* 널 여유 */

    mysql_stmt_bind_result(stmt, &result);

    /* 결과 fetch */
    int fetch_status = mysql_stmt_fetch(stmt);
    mysql_stmt_close(stmt);

    if (fetch_status == MYSQL_NO_DATA)
        return 1; /* userid 없음 */
    if (fetch_status) /* 기타 오류 */
        return -4;

    return 0; /* 성공 */
}

int user_repository_get_info(const char *userid, struct user_info *out) {
    MYSQL *db_conn = get_db();
    if (!db_conn) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    const char *sql = "SELECT userid, nickname FROM users WHERE userid = ?";
    if (mysql_stmt_prepare(st, sql, strlen(sql))) {
        mysql_stmt_close(st);
        return -2;
    }

    unsigned long uid_len = strlen(userid);
    MYSQL_BIND p = {0};
    p.buffer_type = MYSQL_TYPE_STRING;
    p.buffer = (char *) userid;
    p.buffer_length = uid_len;
    p.length = &uid_len;
    mysql_stmt_bind_param(st, &p);

    if (mysql_stmt_execute(st)) {
        mysql_stmt_close(st);
        return -3;
    }

    MYSQL_BIND r[2] = {0};
    unsigned long len1 = sizeof out->userid - 1;
    unsigned long len2 = sizeof out->nickname - 1;
    r[0].buffer_type = r[1].buffer_type = MYSQL_TYPE_STRING;
    r[0].buffer = out->userid;
    r[0].buffer_length = len1;
    r[0].length = &len1;
    r[1].buffer = out->nickname;
    r[1].buffer_length = len2;
    r[1].length = &len2;
    mysql_stmt_bind_result(st, r);

    int fs = mysql_stmt_fetch(st);
    mysql_stmt_close(st);
    if (fs == MYSQL_NO_DATA) return 1;
    if (fs) return -4;

    out->userid[len1] = '\0';
    out->nickname[len2] = '\0';
    return 0;
}

int user_repository_get_id_by_userid(const char *uid, uint32_t *out_id)
{
    MYSQL *db = get_db();
    MYSQL_STMT *st = mysql_stmt_init(db);
    const char *q = "SELECT id FROM users WHERE userid=? LIMIT 1";
    if (mysql_stmt_prepare(st,q,strlen(q))) goto err;

    MYSQL_BIND b[1]={0};
    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (void*)uid;
    unsigned long len=strlen(uid); b[0].length=&len;

    if (mysql_stmt_bind_param(st,b) || mysql_stmt_execute(st)) goto err;

    MYSQL_BIND rb={0}; uint32_t id; rb.buffer_type=MYSQL_TYPE_LONG; rb.buffer=&id;
    if (mysql_stmt_bind_result(st,&rb) || mysql_stmt_fetch(st)) goto err;

    *out_id=id; mysql_stmt_close(st); return 0;
    err:
        if(st) mysql_stmt_close(st); return -1;
}

int user_repository_get_info_by_id(uint32_t id,
                                   struct user_info *out)
{
    MYSQL *db = get_db();
    if (!db) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db);
    const char *sql =
        "SELECT userid, nickname "
        "FROM users WHERE id = ? LIMIT 1";

    if (mysql_stmt_prepare(st, sql, strlen(sql))) goto err;

    /* -------- 파라미터(id) 바인드 -------- */
    MYSQL_BIND pb = {0};
    pb.buffer_type = MYSQL_TYPE_LONG;
    pb.buffer      = &id;
    pb.is_unsigned = 1;

    if (mysql_stmt_bind_param(st, &pb) || mysql_stmt_execute(st)) goto err;

    /* -------- 결과 바인드 -------- */
    char uid_buf[USERID_MAX + 1]   = {0};
    char nick_buf[NICK_MAX + 1]    = {0};
    unsigned long uid_len = 0, nick_len = 0;

    MYSQL_BIND rb[2] = {0};
    rb[0].buffer_type   = MYSQL_TYPE_STRING;
    rb[0].buffer        = uid_buf;
    rb[0].buffer_length = USERID_MAX;
    rb[0].length        = &uid_len;

    rb[1].buffer_type   = MYSQL_TYPE_STRING;
    rb[1].buffer        = nick_buf;
    rb[1].buffer_length = NICK_MAX;
    rb[1].length        = &nick_len;

    if (mysql_stmt_bind_result(st, rb)) goto err;

    int fs = mysql_stmt_fetch(st);
    mysql_stmt_close(st);

    if (fs == 0) {                          /* 검색 성공 */
        strncpy(out->userid,   uid_buf,   USERID_MAX);
        out->userid[USERID_MAX] = '\0';
        strncpy(out->nickname, nick_buf,  NICK_MAX);
        out->nickname[NICK_MAX] = '\0';
        return 0;
    }
    if (fs == MYSQL_NO_DATA)                /* 존재하지 않음 */
        return 1;

    return -1;                              /* 그 외 fetch 오류 */

    err:
        if (st) mysql_stmt_close(st);
    return -1;
}
