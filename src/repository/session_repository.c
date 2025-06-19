#include "repository/session_repository.h"

#include <stdio.h>

#include <mysql/mysql.h>
#include <string.h>

#include "db.h"

#define UID_MAX_LEN 255

/*------------------------------------------------------------------*/
/* 세션 추가                                                         */
/*------------------------------------------------------------------*/
int session_repository_add(const char *sid, uint32_t user_id, time_t exp) {
    MYSQL *db = get_db();
    if (!db) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db);
    const char *sql =
            "INSERT INTO sessions(id, userid, expires_at) "
            "VALUES( ?, ?, FROM_UNIXTIME(?) )";

    if (mysql_stmt_prepare(st, sql, strlen(sql))) goto prep_err;

    MYSQL_BIND b[3] = {0};

    /* id (CHAR(64)) ---------------------------------------------------- */
    unsigned long id_len = strlen(sid);
    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (char *) sid;
    b[0].buffer_length = id_len;
    b[0].length = &id_len;

    /* userid -> numeric id (INT UNSIGNED) ------------------------------ */
    b[1].buffer_type = MYSQL_TYPE_LONG;
    b[1].buffer = &user_id;
    b[1].is_unsigned = 1;

    /* expires_at ------------------------------------------------------- */
    b[2].buffer_type = MYSQL_TYPE_LONGLONG;
    b[2].buffer = &exp;
    /* time_t 는 signed ⇒ is_unsigned = 0 */

    if (mysql_stmt_bind_param(st, b)) goto bind_err;
    if (mysql_stmt_execute(st)) goto exec_err;

    mysql_stmt_close(st);
    return 0; /* 성공 */

prep_err:
    fprintf(stderr, "[prepare] %u : %s\n",
            mysql_stmt_errno(st), mysql_stmt_error(st));
    goto fail;
bind_err:
    fprintf(stderr, "[bind] %u : %s\n",
            mysql_stmt_errno(st), mysql_stmt_error(st));
    goto fail;
exec_err:
    fprintf(stderr, "[exec] %u : %s\n",
            mysql_stmt_errno(st), mysql_stmt_error(st));
    goto fail;

fail:
    mysql_stmt_close(st);
    return -1;
}

/*------------------------------------------------------------------*/
/* 세션 조회                                                         */
/*------------------------------------------------------------------*/
int session_repository_find_id(const char *sid,
                               uint32_t *out_user_id,
                               time_t *out_exp) /* NULL 허용 */
{
    MYSQL *db = get_db();
    if (!db) return -1;

    MYSQL_STMT *st = mysql_stmt_init(db);
    const char *sql =
            "SELECT userid, UNIX_TIMESTAMP(expires_at) "
            "FROM sessions WHERE id = ? LIMIT 1";

    if (mysql_stmt_prepare(st, sql, strlen(sql))) {
        mysql_stmt_close(st);
        return -2;
    }

    /* 파라미터 바인딩 (sid) */
    MYSQL_BIND pb = {0};
    pb.buffer_type = MYSQL_TYPE_STRING;
    pb.buffer = (char *) sid;
    pb.buffer_length = 64;
    mysql_stmt_bind_param(st, &pb);

    if (mysql_stmt_execute(st)) {
        mysql_stmt_close(st);
        return -3;
    }

    /* 결과 바인딩 */
    uint32_t uid_val = 0;
    time_t exp_val = 0;

    MYSQL_BIND rb[2] = {0};
    rb[0].buffer_type = MYSQL_TYPE_LONG;
    rb[0].buffer = &uid_val;
    rb[0].is_unsigned = 1;

    rb[1].buffer_type = MYSQL_TYPE_LONGLONG;
    rb[1].buffer = &exp_val;

    mysql_stmt_bind_result(st, rb);

    int fs = mysql_stmt_fetch(st);
    mysql_stmt_close(st);

    if (fs == MYSQL_NO_DATA) return 1; /* 세션 없음 */
    if (fs) return -4; /* fetch 오류 */

    if (out_user_id) *out_user_id = uid_val;
    if (out_exp) *out_exp = exp_val;
    return 0; /* 성공 */
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
