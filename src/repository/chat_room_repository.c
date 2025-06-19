#include <mysql.h>

#include "chat_room_repository.h"
#include <string.h>

#include "db.h"

int chat_room_create(enum room_type type,
                     const char *title,
                     uint32_t creator_id,
                     const uint32_t *extra_members,
                     size_t mem_cnt,
                     uint32_t *out_room_id) {
    MYSQL *db_conn = get_db();
    MYSQL_STMT *stmt = mysql_stmt_init(db_conn);
    const char *q1 = "INSERT INTO chat_room(room_type,title,creator_id)"
            "VALUES(?,?,?)";
    if (mysql_stmt_prepare(stmt, q1, strlen(q1)))
        goto err;

    int rt = (type == ROOM_PUBLIC) ? 1 : 0;
    MYSQL_BIND b1[3] = {0};

    b1[0].buffer_type = MYSQL_TYPE_LONG;
    b1[0].buffer = &rt;
    b1[1].buffer_type = MYSQL_TYPE_STRING;
    b1[1].buffer = (void *) title;
    unsigned long tlen = strlen(title);
    b1[1].length = &tlen;
    b1[2].buffer_type = MYSQL_TYPE_LONG;
    b1[2].buffer = &creator_id;

    if (mysql_stmt_bind_param(stmt, b1) || mysql_stmt_execute(stmt))
        goto err;
    *out_room_id = (uint32_t) mysql_insert_id(db_conn);
    mysql_stmt_close(stmt);

    /* 1‑A‑2) 멤버 등록 (자기 자신 + extra) */
    const char *q2 = "INSERT INTO chat_room_member(room_id,user_id) VALUES (?,?)";
    stmt = mysql_stmt_init(db_conn);
    if (mysql_stmt_prepare(stmt, q2, strlen(q2))) goto err;

    MYSQL_BIND b2[2] = {0};
    b2[0].buffer_type = MYSQL_TYPE_LONG;
    b2[0].buffer = out_room_id;
    b2[1].buffer_type = MYSQL_TYPE_LONG;

    /* 자기 자신 */
    b2[1].buffer = &creator_id;
    if (mysql_stmt_bind_param(stmt, b2) || mysql_stmt_execute(stmt)) goto err;

    /* 나머지 */
    for (size_t i = 0; i < mem_cnt; ++i) {
        b2[1].buffer = (void *) &extra_members[i];
        if (mysql_stmt_bind_param(stmt, b2) || mysql_stmt_execute(stmt)) goto err;
    }
    mysql_stmt_close(stmt);
    return 0;
err:
    if (stmt) mysql_stmt_close(stmt);
    return -1;
}

int chat_room_list_lobby(uint32_t me,
                         struct lobby_room *out, size_t max, size_t *cnt) {
    MYSQL *db_conn = get_db();
    const char *q =
            "SELECT r.id,r.title,"
            "       COALESCE(m.content,''),"
            "       COALESCE(m.sender_id,0),"
            "       COALESCE(UNIX_TIMESTAMP(m.created_at),0),"
            "       COALESCE(u.unread_cnt,0)"
            "FROM chat_room_member rm"
            " JOIN chat_room r ON r.id=rm.room_id"
            " LEFT JOIN ("
            "    SELECT room_id, content, sender_id, created_at"
            "    FROM chat_message"
            "    WHERE (room_id, id) IN ("
            "        SELECT room_id, MAX(id) FROM chat_message GROUP BY room_id"
            "    )"
            ") m ON m.room_id=r.id"
            " LEFT JOIN ("
            "    SELECT c.room_id, COUNT(*) unread_cnt"
            "    FROM chat_message_unread u"
            "    JOIN chat_message c ON c.id=u.message_id"
            "    WHERE u.user_id=?"
            "    GROUP BY c.room_id"
            ") u ON u.room_id=r.id"
            " WHERE rm.user_id=?"
            " ORDER BY m.created_at DESC";

    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    MYSQL_BIND inb[2] = {0};
    inb[0].buffer_type = inb[1].buffer_type = MYSQL_TYPE_LONG;
    inb[0].buffer = inb[1].buffer = &me;
    if (mysql_stmt_bind_param(st, inb) || mysql_stmt_execute(st)) goto err;

    MYSQL_BIND outb[6] = {0};
    uint32_t id, sender, ts, unread;
    char title[81], content[256];
    unsigned long tl, cl;

    outb[0].buffer_type = MYSQL_TYPE_LONG;
    outb[0].buffer = &id;
    outb[1].buffer_type = MYSQL_TYPE_STRING;
    outb[1].buffer = title;
    outb[1].buffer_length = 81;
    outb[1].length = &tl;
    outb[2].buffer_type = MYSQL_TYPE_STRING;
    outb[2].buffer = content;
    outb[2].buffer_length = 256;
    outb[2].length = &cl;
    outb[3].buffer_type = MYSQL_TYPE_LONG;
    outb[3].buffer = &sender;
    outb[4].buffer_type = MYSQL_TYPE_LONG;
    outb[4].buffer = &ts;
    outb[5].buffer_type = MYSQL_TYPE_LONG;
    outb[5].buffer = &unread;
    if (mysql_stmt_bind_result(st, outb)) goto err;

    *cnt = 0;
    while (mysql_stmt_fetch(st) == 0 && *cnt < max) {
        struct lobby_room *d = &out[(*cnt)++];
        d->id = id;
        d->last_sender = sender;
        d->last_time = ts;
        d->unread = unread;
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

int chat_room_list_public(size_t page,
                          struct public_room *out, size_t max, size_t *cnt) {
    MYSQL *db_conn = get_db();
    const char *q =
            "SELECT r.id,r.title,"
            " (SELECT COUNT(*) FROM chat_room_member WHERE room_id=r.id)"
            " AS member_cnt"
            " FROM chat_room r"
            " WHERE r.room_type='PUBLIC'"
            " ORDER BY r.created_at DESC"
            " LIMIT 50 OFFSET ?";

    MYSQL_STMT *st = mysql_stmt_init(db_conn);
    if (mysql_stmt_prepare(st, q, strlen(q))) goto err;

    uint32_t off = (uint32_t) (page * 50);
    MYSQL_BIND inb = {0};
    inb.buffer_type = MYSQL_TYPE_LONG;
    inb.buffer = &off;
    if (mysql_stmt_bind_param(st, &inb) || mysql_stmt_execute(st)) goto err;

    uint32_t id, mc;
    char title[81];
    unsigned long tl;
    MYSQL_BIND outb[3] = {0};
    outb[0].buffer_type = MYSQL_TYPE_LONG;
    outb[0].buffer = &id;
    outb[1].buffer_type = MYSQL_TYPE_STRING;
    outb[1].buffer = title;
    outb[1].buffer_length = 81;
    outb[1].length = &tl;
    outb[2].buffer_type = MYSQL_TYPE_LONG;
    outb[2].buffer = &mc;
    if (mysql_stmt_bind_result(st, outb)) goto err;

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
