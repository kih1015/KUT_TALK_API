#include "repository/session_repository.h"

#include <stdio.h>

#include <mysql/mysql.h>
#include <string.h>

#include "db.h"

#define UID_MAX_LEN 255

/*------------------------------------------------------------------*/
/* 세션 추가                                                         */
/*------------------------------------------------------------------*/
int session_repository_add(const char *sid, const char *uid, time_t exp) {
    MYSQL *db_conn = get_db();
    if (!db_conn) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    const char *sql =
            "INSERT INTO sessions(id,userid,expires_at) "
            "VALUES( ?, ?, FROM_UNIXTIME(?) )";

    if (mysql_stmt_prepare(st, sql, strlen(sql))) {
        fprintf(stderr, "[prepare] %u : %s\n",
                mysql_stmt_errno(st), mysql_stmt_error(st));
        mysql_stmt_close(st);
        return -2;
    }

    /* ---------- 파라미터 바인딩 ---------- */
    MYSQL_BIND b[3];
    memset(b, 0, sizeof b);

    unsigned long id_len = strlen(sid); /* ★ 실제 길이 */
    unsigned long uid_len = strlen(uid);

    /* id (CHAR(64)) */
    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (char *) sid;
    b[0].buffer_length = id_len;
    b[0].length = &id_len;

    /* userid (VARCHAR) */
    b[1].buffer_type = MYSQL_TYPE_STRING;
    b[1].buffer = (char *) uid;
    b[1].buffer_length = uid_len;
    b[1].length = &uid_len;

    /* expires_at (time_t → FROM_UNIXTIME) */
    b[2].buffer_type = MYSQL_TYPE_LONGLONG;
    b[2].buffer = &exp;
    b[2].buffer_length = sizeof(exp);
    b[2].is_unsigned = 0; /* time_t 는 signed */

    if (mysql_stmt_bind_param(st, b)) {
        fprintf(stderr, "[bind] %u : %s\n",
                mysql_stmt_errno(st), mysql_stmt_error(st));
        mysql_stmt_close(st);
        return -3;
    }

    /* ---------- 실행 ---------- */
    if (mysql_stmt_execute(st)) {
        fprintf(stderr, "[exec] %u : %s\n",
                mysql_stmt_errno(st), mysql_stmt_error(st));
        int err = mysql_stmt_errno(st);
        mysql_stmt_close(st);
        return err ? -4 : -5;
    }

    mysql_stmt_close(st);
    return 0; /* 성공 */
}

/*------------------------------------------------------------------*/
/* 세션 조회                                                         */
/*------------------------------------------------------------------*/
int session_repository_find(const char *sid, char *out_uid, time_t *out_exp) /* NULL 허용 */
{
    MYSQL *db_conn = get_db();
    if (!db_conn) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    const char *sql =
            "SELECT userid, UNIX_TIMESTAMP(expires_at) "
            "FROM sessions WHERE id = ?";

    if (mysql_stmt_prepare(st, sql, strlen(sql))) {
        mysql_stmt_close(st);
        return -2;
    }

    /* 파라미터 바인드 */
    MYSQL_BIND p = {0};
    p.buffer_type = MYSQL_TYPE_STRING;
    p.buffer = (char *) sid;
    p.buffer_length = 64;
    mysql_stmt_bind_param(st, &p);

    if (mysql_stmt_execute(st)) {
        mysql_stmt_close(st);
        return -3;
    }

    /* 결과 바인드 */
    char uid_buf[UID_MAX_LEN + 1] = {0};
    time_t exp_val = 0;

    MYSQL_BIND r[2] = {0};
    r[0].buffer_type = MYSQL_TYPE_STRING;
    r[0].buffer = uid_buf;
    r[0].buffer_length = UID_MAX_LEN;

    r[1].buffer_type = MYSQL_TYPE_LONGLONG;
    r[1].buffer = &exp_val;

    mysql_stmt_bind_result(st, r);

    int fetch_status = mysql_stmt_fetch(st);
    mysql_stmt_close(st);

    if (fetch_status == MYSQL_NO_DATA)
        return 1; /* 존재하지 않음 */
    if (fetch_status) /* 오류 */
        return -4;

    /* 호출자 버퍼에 복사 */
    if (out_uid) strncpy(out_uid, uid_buf, UID_MAX_LEN + 1);
    if (out_exp) *out_exp = exp_val;

    return 0;
}

/*------------------------------------------------------------------*/
/* 세션 삭제                                                         */
/*------------------------------------------------------------------*/
int session_repository_delete(const char *sid) {
    MYSQL *db_conn = get_db();
    if (!db_conn) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    const char *sql = "DELETE FROM sessions WHERE id = ?";

    if (mysql_stmt_prepare(st, sql, strlen(sql))) {
        mysql_stmt_close(st);
        return -2;
    }

    MYSQL_BIND p = {0};
    p.buffer_type = MYSQL_TYPE_STRING;
    p.buffer = (char *) sid;
    p.buffer_length = 64;
    mysql_stmt_bind_param(st, &p);

    if (mysql_stmt_execute(st)) {
        int err = mysql_errno(db_conn);
        mysql_stmt_close(st);
        return err ? -3 : 1; /* 1행도 영향 없으면 1 */
    }

    unsigned long affected = mysql_stmt_affected_rows(st);
    mysql_stmt_close(st);
    return affected ? 0 : 1; /* 0 삭제 성공, 1 없음 */
}
