#include <mysql.h>

#include "chat_repository.h"

#include <stdio.h>
#include <string.h>

#include "db.h"

int chat_room_create(
    enum room_type type,
    const char *title,
    uint32_t creator_id,
    const uint32_t *extra_members,
    size_t mem_cnt,
    uint32_t *out_room_id
) {
    MYSQL *db = get_db();
    MYSQL_STMT *st = mysql_stmt_init(db);

    /* 1) chat_room INSERT (열 명시) */
    const char *q1 =
            "INSERT INTO chat_room (title, room_type, creator_id) VALUES (?,?,?)";

    if (mysql_stmt_prepare(st, q1, strlen(q1))) {
        fprintf(stderr, "prepare‑1: %s\n", mysql_stmt_error(st));
        fflush(stderr);
        goto err;
    }

    /* 바인딩 */
    const char *rt = (type == ROOM_PUBLIC) ? "PUBLIC" : "PRIVATE";
    unsigned long t_len = strlen(title), rt_len = strlen(rt);

    MYSQL_BIND b[3] = {0};
    b[0].buffer_type = MYSQL_TYPE_STRING;
    b[0].buffer = (void *) title;
    b[0].length = &t_len;

    b[1].buffer_type = MYSQL_TYPE_STRING;
    b[1].buffer = (void *) rt;
    b[1].length = &rt_len;

    b[2].buffer_type = MYSQL_TYPE_LONG;
    b[2].buffer = &creator_id;

    if (mysql_stmt_bind_param(st, b) || mysql_stmt_execute(st)) {
        fprintf(stderr, "execute‑1: %s\n", mysql_stmt_error(st));
        fflush(stderr);
        goto err;
    }

    uint32_t rid = (uint32_t) mysql_insert_id(db);
    *out_room_id = rid;
    mysql_stmt_close(st);

    /* 2) chat_room_member INSERT */
    const char *q2 =
            "INSERT INTO chat_room_member (room_id,user_id) VALUES (?,?)";

    st = mysql_stmt_init(db);
    if (mysql_stmt_prepare(st, q2, strlen(q2))) {
        fprintf(stderr, "prepare‑2: %s\n", mysql_stmt_error(st));
        goto err;
    }

    MYSQL_BIND mb[2] = {0};
    mb[0].buffer_type = MYSQL_TYPE_LONG;
    mb[0].buffer = &rid;
    uint32_t uid;
    mb[1].buffer_type = MYSQL_TYPE_LONG;
    mb[1].buffer = &uid;

    /* 개설자 */
    uid = creator_id;
    if (mysql_stmt_bind_param(st, mb) || mysql_stmt_execute(st)) {
        fprintf(stderr, "execute‑2a: %s\n", mysql_stmt_error(st));
        goto err;
    }

    /* 추가 멤버 */
    for (size_t i = 0; i < mem_cnt; ++i) {
        uid = extra_members[i];
        if (mysql_stmt_bind_param(st, mb) || mysql_stmt_execute(st)) {
            fprintf(stderr, "execute‑2b: %s\n", mysql_stmt_error(st));
            goto err;
        }
    }
    mysql_stmt_close(st);
    return 0;

err:
    if (st) mysql_stmt_close(st);
    return -1;
}

int chat_room_list_lobby(uint32_t me,
                         struct lobby_room *out, size_t max, size_t *cnt) {
    MYSQL *db = get_db();
    const char *q =
            "SELECT r.id, r.title, "
            "       COALESCE(m.content,''), "
            "       COALESCE(m.sender_id,0), "
            "       COALESCE(UNIX_TIMESTAMP(m.created_at),0), "
            "       COALESCE(u.unread_cnt,0), "
            "       (SELECT COUNT(*) FROM chat_room_member WHERE room_id = r.id) "
            "         AS member_cnt "
            "FROM   chat_room_member rm "
            "JOIN   chat_room r ON r.id = rm.room_id "
            "LEFT JOIN ("
            "   SELECT room_id, content, sender_id, created_at "
            "   FROM   chat_message "
            "   WHERE  (room_id, id) IN ("
            "       SELECT room_id, MAX(id) FROM chat_message GROUP BY room_id"
            "   )"
            ") m ON m.room_id = r.id "
            "LEFT JOIN ("
            "   SELECT c.room_id, COUNT(*) unread_cnt "
            "   FROM   chat_message_unread u "
            "   JOIN   chat_message c ON c.id = u.message_id "
            "   WHERE  u.user_id = ? "
            "   GROUP  BY c.room_id"
            ") u ON u.room_id = r.id "
            "WHERE  rm.user_id = ? "
            "ORDER  BY m.created_at DESC";

    MYSQL_STMT *st = mysql_stmt_init(db);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    /* IN 파라미터 두 개 모두 me */
    MYSQL_BIND inb[2] = {0};
    inb[0].buffer_type = inb[1].buffer_type = MYSQL_TYPE_LONG;
    inb[0].buffer = inb[1].buffer = &me;

    if (mysql_stmt_bind_param(st, inb) || mysql_stmt_execute(st)) goto err;

    /* ---------- 결과 바인드 ---------- */
    uint32_t id, sender, ts, unread, members;
    char title[81], content[256];
    unsigned long tl, cl;

    MYSQL_BIND rb[7] = {0};
    rb[0].buffer_type = MYSQL_TYPE_LONG;
    rb[0].buffer = &id;
    rb[1].buffer_type = MYSQL_TYPE_STRING;
    rb[1].buffer = title;
    rb[1].buffer_length = 81;
    rb[1].length = &tl;
    rb[2].buffer_type = MYSQL_TYPE_STRING;
    rb[2].buffer = content;
    rb[2].buffer_length = 256;
    rb[2].length = &cl;
    rb[3].buffer_type = MYSQL_TYPE_LONG;
    rb[3].buffer = &sender;
    rb[4].buffer_type = MYSQL_TYPE_LONG;
    rb[4].buffer = &ts;
    rb[5].buffer_type = MYSQL_TYPE_LONG;
    rb[5].buffer = &unread;
    rb[6].buffer_type = MYSQL_TYPE_LONG;
    rb[6].buffer = &members;

    if (mysql_stmt_bind_result(st, rb)) goto err;

    *cnt = 0;
    while (mysql_stmt_fetch(st) == 0 && *cnt < max) {
        struct lobby_room *d = &out[(*cnt)++];
        d->id = id;
        d->last_sender = sender;
        d->last_time = ts;
        d->unread = unread;
        d->member_cnt = members;

        strncpy(d->title, title, 80);
        d->title[80] = '\0';
        strncpy(d->last_content, content, 255);
        d->last_content[255] = '\0';
    }
    mysql_stmt_close(st);
    return 0;

err:
    if (st) mysql_stmt_close(st);
    return -1;
}

int chat_room_list_public(struct public_room *out, size_t max, size_t *cnt) {
    MYSQL *db = get_db();
    if (!db) return -1;

    /* ▼ LIMIT / OFFSET 없이 모든 PUBLIC 방 조회 */
    const char *q =
            "SELECT r.id, r.title, "
            "       (SELECT COUNT(*) FROM chat_room_member WHERE room_id = r.id)"
            "       AS member_cnt "
            "FROM   chat_room r "
            "WHERE  r.room_type = 'PUBLIC' "
            "ORDER  BY r.created_at DESC";

    MYSQL_STMT *st = mysql_stmt_init(db);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    if (mysql_stmt_execute(st)) goto err;

    /* -------- 결과 바인딩 -------- */
    uint32_t id, mc;
    char title[81];
    unsigned long tl = 0;
    MYSQL_BIND rb[3] = {0};

    rb[0].buffer_type = MYSQL_TYPE_LONG;
    rb[0].buffer = &id;

    rb[1].buffer_type = MYSQL_TYPE_STRING;
    rb[1].buffer = title;
    rb[1].buffer_length = sizeof title;
    rb[1].length = &tl;

    rb[2].buffer_type = MYSQL_TYPE_LONG;
    rb[2].buffer = &mc;

    if (mysql_stmt_bind_result(st, rb)) goto err;

    *cnt = 0;
    while (mysql_stmt_fetch(st) == 0 && *cnt < max) {
        out[*cnt].id = id;
        out[*cnt].member_cnt = mc;
        strncpy(out[*cnt].title, title, 80);
        out[*cnt].title[80] = '\0';
        (*cnt)++;
    }
    mysql_stmt_close(st);
    return 0;

err:
    if (st) mysql_stmt_close(st);
    return -1;
}

int chat_room_join(uint32_t room, uint32_t user) {
    MYSQL *db_conn = get_db();
    const char *q = "INSERT IGNORE INTO chat_room_member(room_id,user_id)"
            "VALUES(?,?)";
    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    MYSQL_BIND b[2] = {0};
    b[0].buffer_type = b[1].buffer_type = MYSQL_TYPE_LONG;
    b[0].buffer = &room;
    b[1].buffer = &user;

    if (mysql_stmt_bind_param(st, b) || mysql_stmt_execute(st)) goto err;

    int dup = (mysql_stmt_affected_rows(st) == 0); /* 이미 존재 */
    mysql_stmt_close(st);
    return dup ? -1 : 0;
err:
    if (st) mysql_stmt_close(st);
    return -2;
}

int chat_room_leave(uint32_t room, uint32_t user) {
    MYSQL *db_conn = get_db();
    const char *q = "DELETE FROM chat_room_member WHERE room_id=? AND user_id=?";
    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    MYSQL_BIND b[2] = {0};
    b[0].buffer_type = b[1].buffer_type = MYSQL_TYPE_LONG;
    b[0].buffer = &room;
    b[1].buffer = &user;

    if (mysql_stmt_bind_param(st, b) || mysql_stmt_execute(st)) goto err;

    int miss = (mysql_stmt_affected_rows(st) == 0); /* 멤버 아님 */
    mysql_stmt_close(st);
    return miss ? -1 : 0;
err:
    if (st) mysql_stmt_close(st);
    return -2;
}

int chat_room_clear_unread(uint32_t user_id, uint32_t room_id) {
    MYSQL *db = get_db();
    const char *q =
            "DELETE u FROM chat_message_unread u "
            "JOIN chat_message m ON m.id = u.message_id "
            "WHERE u.user_id = ? AND m.room_id = ?";

    MYSQL_STMT *st = mysql_stmt_init(db);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    MYSQL_BIND b[2] = {0};
    b[0].buffer_type = b[1].buffer_type = MYSQL_TYPE_LONG;
    b[0].buffer = &user_id;
    b[1].buffer = &room_id;

    if (mysql_stmt_bind_param(st, b) || mysql_stmt_execute(st)) goto err;

    mysql_stmt_close(st);
    return 0;
err:
    if (st) mysql_stmt_close(st);
    return -1;
}

int chat_message_list(
    uint32_t room_id,
    size_t page,
    size_t limit,
    struct chat_msg_dto *out,
    size_t *out_cnt
) {
    MYSQL *db = get_db();
    if (!db) return -1;

    const char *sql =
            "SELECT m.id, m.sender_id, u.nickname, m.content, "
            "       UNIX_TIMESTAMP(m.created_at), "
            "       (SELECT COUNT(*) FROM chat_message_unread "
            "         WHERE message_id = m.id) AS unread_cnt "
            "FROM   chat_message m "
            "JOIN   users u ON u.id = m.sender_id "
            "WHERE  m.room_id = ? "
            "ORDER  BY m.id DESC "
            "LIMIT  ? OFFSET ?";

    MYSQL_STMT *st = mysql_stmt_init(db);
    if (mysql_stmt_prepare(st, sql, strlen(sql))) goto err;

    uint32_t off = (uint32_t) (page * limit);
    MYSQL_BIND pb[3] = {0};
    pb[0].buffer_type = MYSQL_TYPE_LONG;
    pb[0].buffer = &room_id;
    pb[1].buffer_type = MYSQL_TYPE_LONG;
    pb[1].buffer = &limit;
    pb[2].buffer_type = MYSQL_TYPE_LONG;
    pb[2].buffer = &off;

    if (mysql_stmt_bind_param(st, pb) || mysql_stmt_execute(st)) goto err;

    /* -------- 결과 바인드 -------- */
    uint32_t id, sender, ts, unread;
    char nick[NICK_MAX + 1];
    unsigned long nlen;
    char content[MSG_MAX + 1];
    unsigned long clen;

    MYSQL_BIND rb[7] = {0};
    rb[0].buffer_type = MYSQL_TYPE_LONG;
    rb[0].buffer = &id;
    rb[1].buffer_type = MYSQL_TYPE_LONG;
    rb[1].buffer = &sender;

    rb[2].buffer_type = MYSQL_TYPE_STRING;
    rb[2].buffer = nick;
    rb[2].buffer_length = sizeof nick;
    rb[2].length = &nlen;

    rb[3].buffer_type = MYSQL_TYPE_STRING;
    rb[3].buffer = content;
    rb[3].buffer_length = sizeof content;
    rb[3].length = &clen;

    rb[4].buffer_type = MYSQL_TYPE_LONG;
    rb[4].buffer = &ts;
    rb[5].buffer_type = MYSQL_TYPE_LONG;
    rb[5].buffer = &unread;

    if (mysql_stmt_bind_result(st, rb)) goto err;

    *out_cnt = 0;
    while (mysql_stmt_fetch(st) == 0 && *out_cnt < limit) {
        struct chat_msg_dto *d = &out[(*out_cnt)++];
        d->id = id;
        d->sender_id = sender;
        d->created_at = ts;
        d->unread_cnt = unread;

        strncpy(d->sender_nick, nick, NICK_MAX);
        d->sender_nick[NICK_MAX] = '\0';

        strncpy(d->content, content, MSG_MAX);
        d->content[MSG_MAX] = '\0';
    }
    mysql_stmt_close(st);
    return 0;

err:
    if (st) mysql_stmt_close(st);
    return -1;
}

int chat_room_is_member(uint32_t room_id, uint32_t user_id)
{
    MYSQL *db = get_db();
    if (!db) return -1;

    static const char *sql =
        "SELECT 1 FROM chat_room_member "
        "WHERE room_id = ? AND user_id = ? LIMIT 1";

    MYSQL_STMT *st = mysql_stmt_init(db);
    if (!st) return -1;
    if (mysql_stmt_prepare(st, sql, strlen(sql))) goto err;

    MYSQL_BIND pb[2] = {0};
    pb[0].buffer_type = pb[1].buffer_type = MYSQL_TYPE_LONG;
    pb[0].buffer = &room_id; pb[0].is_unsigned = 1;
    pb[1].buffer = &user_id; pb[1].is_unsigned = 1;

    if (mysql_stmt_bind_param(st, pb) || mysql_stmt_execute(st)) goto err;

    /* 결과 한 행이라도 있으면 멤버 */
    int dummy;
    MYSQL_BIND rb = {0};
    rb.buffer_type = MYSQL_TYPE_LONG;
    rb.buffer      = &dummy;

    if (mysql_stmt_bind_result(st, &rb)) goto err;

    int fetch_rc = mysql_stmt_fetch(st);
    mysql_stmt_close(st);

    if (fetch_rc == 0)            return 0; /* 멤버 */
    if (fetch_rc == MYSQL_NO_DATA) return 1; /* 아니면 */
    return -1;                                /* fetch 오류 */

    err:
        mysql_stmt_close(st);
    return -1;
}

int chat_repo_count_members(uint32_t room_id, size_t *out_cnt) {
    MYSQL *db = get_db();
    const char *q = "SELECT COUNT(*) FROM chat_room_member WHERE room_id = ?";
    MYSQL_STMT *st = mysql_stmt_init(db);
    if (!st) return -1;

    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    // bind input param
    MYSQL_BIND pb[1] = {0};
    pb[0].buffer_type   = MYSQL_TYPE_LONG;
    pb[0].buffer        = &room_id;
    pb[0].is_unsigned   = 1;
    if (mysql_stmt_bind_param(st, pb) || mysql_stmt_execute(st))
        goto err;

    // bind result
    unsigned long cnt = 0;
    MYSQL_BIND rb[1] = {0};
    rb[0].buffer_type = MYSQL_TYPE_LONG;
    rb[0].buffer      = &cnt;
    rb[0].is_unsigned = 1;
    if (mysql_stmt_bind_result(st, rb)) goto err;
    if (mysql_stmt_fetch(st) != 0)    goto err;

    *out_cnt = (size_t)cnt;
    mysql_stmt_close(st);
    return 0;

    err:
        if (st) mysql_stmt_close(st);
    return -1;
}

int chat_repo_delete_room(uint32_t room_id) {
    MYSQL *db = get_db();
    const char *q = "DELETE FROM chat_room WHERE id = ?";
    MYSQL_STMT *st = mysql_stmt_init(db);
    if (!st) return -1;

    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    // bind input param
    MYSQL_BIND pb[1] = {0};
    pb[0].buffer_type   = MYSQL_TYPE_LONG;
    pb[0].buffer        = &room_id;
    pb[0].is_unsigned   = 1;
    if (mysql_stmt_bind_param(st, pb) || mysql_stmt_execute(st))
        goto err;

    mysql_stmt_close(st);
    return 0;

    err:
        if (st) mysql_stmt_close(st);
    return -1;
}
