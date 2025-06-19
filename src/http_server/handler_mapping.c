/* http_server/handler_mapping.c
   ──────────────────────────── */
#define GNU_SOURCE
#include "handler_mapping.h"
#include "http_server/http_request.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <regex.h>

#include "controller/user_controller.h"
#include "controller/chat_controller.h"
#include "repository/db.h"

#define BUF_SIZE 4096

/* -------- 한 배열에 모두 정의 -------- */
struct route ROUTES[] = {
    /* --- 고정 --- */
    {"POST", "/users/login", user_controller_login, 0},
    {"POST", "/users", user_controller_register, 0},
    {"GET", "/users/me", user_controller_get_me, 0},
    {"POST", "/users/logout", user_controller_logout, 0},
    {"POST", "/chat/rooms", chat_controller_create_room, 0},
    {"GET", "/chat/rooms/me", chat_controller_my_rooms, 0},
    {"GET", "/chat/rooms/public", chat_controller_list_public, 0},

    /* --- 정규식 --- */
    {"POST", "^/chat/rooms/([0-9]+)/join$", chat_controller_join, 1},
    {"DELETE", "^/chat/rooms/([0-9]+)/member$", chat_controller_leave, 1},
    {"GET", "^/chat/rooms/([0-9]+)/messages$", chat_controller_get_messages, 1},
};
static const size_t ROUTE_CNT = sizeof ROUTES / sizeof *ROUTES;

/* -------- 컴파일러 초기화 -------- */
__attribute__((constructor))
static void route_compile(void) {
    for (size_t i = 0; i < ROUTE_CNT; ++i) {
        if (ROUTES[i].is_regex) {
            int rc = regcomp(&ROUTES[i].re, ROUTES[i].pattern, REG_EXTENDED);
            if (rc) {
                char err[128];
                regerror(rc, &ROUTES[i].re, err, sizeof err);
                fprintf(stderr, "regex compile fail %s : %s\n",
                        ROUTES[i].pattern, err);
                exit(1);
            }
        }
    }
}

/* -------- 숫자 캡처 → uint -------- */
static int cap_uint(const char *s, const regmatch_t *m, uint32_t *out) {
    if (m->rm_so < 0) return -1;
    char buf[16];
    size_t n = m->rm_eo - m->rm_so;
    if (n == 0 || n >= sizeof buf) return -1;
    memcpy(buf, s + m->rm_so, n);
    buf[n] = '\0';
    char *e;
    unsigned long v = strtoul(buf, &e, 10);
    if (*e) return -1;
    *out = (uint32_t) v;
    return 0;
}

/* ───── 고정 경로 비교: '?' 이후는 무시 ───── */
static int path_equals(const char *path, const char *literal) {
    size_t lit_len = strlen(literal);
    return strncmp(path, literal, lit_len) == 0 &&
           (path[lit_len] == '\0' || path[lit_len] == '?');
}

/* -------- 스레드 핸들러 -------- */
void *handle_client_thread(void *arg) {
    if (db_thread_init() != 0) {
        close(*(int *) arg);
        free(arg);
        return NULL;
    }

    int fd = *(int *) arg;
    free(arg);
    char buf[BUF_SIZE];
    int n = read(fd, buf,BUF_SIZE - 1);
    if (n <= 0) {
        close(fd);
        db_thread_cleanup();
        return NULL;
    }
    buf[n] = '\0';

    struct http_request req;
    if (parse_http_request(buf, n, &req) != 0) {
        write(fd, "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n", 50);
        close(fd);
        db_thread_cleanup();
        return NULL;
    }

    char resp[8192];
    int rlen = -1;
    for (size_t i = 0; i < ROUTE_CNT && rlen == -1; ++i) {
        if (strcmp(req.method, ROUTES[i].method) != 0) continue;

        if (!ROUTES[i].is_regex) {
            /* 고정 */
            if (path_equals(req.path, ROUTES[i].pattern))
                rlen = ROUTES[i].handler(&req, resp, sizeof resp);
        } else {
            /* 정규식 */
            const char *qmark = strchr(req.path, '?');
            size_t plen = qmark
                              ? (size_t) (qmark - req.path)
                              : strlen(req.path);

            char path_only[512];
            if (plen >= sizeof path_only) plen = sizeof path_only - 1;
            memcpy(path_only, req.path, plen);
            path_only[plen] = '\0';

            /* 1) 정규식 매칭 */
            regmatch_t m[2];
            if (regexec(&ROUTES[i].re, path_only, 2, m, 0) == 0) {
                /* 2) room_id 캡처 → uint32_t */
                uint32_t room;
                if (cap_uint(path_only, &m[1], &room) != 0) break;

                /* 3) 안전한 전달: 힙에 복사 */
                char idbuf[16];
                sprintf(idbuf, "%u", room);
                char *idheap = strdup(idbuf);
                if (!idheap) {
                    rlen = -2;
                    break;
                }

                struct http_request copy = req; /* 전체 요청은 그대로 유지 */
                copy.body = idheap; /* room_id 임시 전달 */

                rlen = ROUTES[i].handler(&copy, resp, sizeof resp);

                free(idheap); /* 사용 직후 해제 */
            }
        }
    }

    /* -------- 응답 -------- */
    if (rlen > 0) write(fd, resp, rlen);
    else if (rlen == -1)
        write(fd, "HTTP/1.1 404 Not Found\r\nContent-Length:0\r\n\r\n", 48);
    else
        write(fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\n\r\n", 57);

    close(fd);
    db_thread_cleanup();
    return NULL;
}
