#include "adapter/user_adapter.h"
#include "handler_mapping.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define BUF_SIZE    4096

const struct route ROUTES[] = {
    {"POST", "/users/login", 0, user_adapter_login},
    {"POST", "/users", 0, user_adapter_register},
    { "GET", "/users/me", 0, user_adapter_get_me }, /* prefix 매칭 */
};
const size_t ROUTE_COUNT = sizeof ROUTES / sizeof *ROUTES;

static const char *ALLOWED_ORIGINS[] = {
    "http://localhost:5173",
    "https://kuttalk.kro.kr"
};

static int is_origin_allowed(const char *origin)
{
    for (size_t i = 0; i < sizeof ALLOWED_ORIGINS/sizeof *ALLOWED_ORIGINS; ++i)
        if (strcmp(origin, ALLOWED_ORIGINS[i]) == 0) return 1;
    return 0;
}

static const char *find_header(const char *req, const char *name)
{
    size_t nlen = strlen(name);
    const char *p = req;
    while ((p = strcasestr(p, name)) != NULL) {
        if (p != req && *(p-1) != '\n' && *(p-1) != '\r') { ++p; continue; }
        const char *val = p + nlen;
        while (*val == ' ' || *val == ':') ++val;
        const char *eol = strpbrk(val, "\r\n");
        static char buf[256];
        size_t len = eol ? (size_t)(eol - val) : 255;
        if (len > 255) len = 255;
        strncpy(buf, val, len); buf[len] = '\0';
        return buf;
    }
    return NULL;
}

static int parse_request_line(const char *buf,
                              char method[8], char path[256]) {
    /* "METHOD SP PATH SP HTTP/1.x" 형식 가정 */
    const char *sp1 = strchr(buf, ' ');
    if (!sp1 || sp1 - buf >= 7) return -1;
    strncpy(method, buf, sp1 - buf);
    method[sp1 - buf] = '\0';

    const char *sp2 = strchr(sp1 + 1, ' ');
    if (!sp2 || sp2 - (sp1 + 1) >= 255) return -1;
    strncpy(path, sp1 + 1, sp2 - (sp1 + 1));
    path[sp2 - (sp1 + 1)] = '\0';
    return 0;
}

void *handle_client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    int len = read(client_fd, buf, BUF_SIZE - 1);
    if (len <= 0) { close(client_fd); return NULL; }
    buf[len] = '\0';

    /* ── 1) 요청 라인 파싱 ───────────────────────────────────────── */
    char method[8], path[256];
    if (parse_request_line(buf, method, path) != 0) {
        write(client_fd,
              "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n", 50);
        close(client_fd);
        return NULL;
    }

    char resp[8192];        /* 공통 응답 버퍼 */
    int  rlen = -1;

    /* ── 2) GET /users/me ───────────────────────────────────────── */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/users/me") == 0) {
        rlen = user_adapter_get_me(buf, resp, sizeof resp);   /* 쿠키 필요 */
    }

    /* ── 3) POST 엔드포인트 ──────────────────────────────────────── */
    else if (strcmp(method, "POST") == 0) {
        const char *body = strstr(buf, "\r\n\r\n");
        if (!body) {
            write(client_fd,
                  "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n", 50);
            close(client_fd);
            return NULL;
        }
        body += 4;

        if (strcmp(path, "/users") == 0)
            rlen = user_adapter_register(body, resp, sizeof resp);
        else if (strcmp(path, "/users/login") == 0)
            rlen = user_adapter_login(body, resp, sizeof resp);
    }

    /* ── 4) 응답 전송 또는 404/500 ───────────────────────────────── */
    if (rlen > 0) {
        /* ① Origin 헤더 파싱 */
        const char *origin = find_header(buf, "Origin");
        int allow = origin && is_origin_allowed(origin);

        if (allow) {
            /* ② "\r\n\r\n" 앞에 CORS 헤더 삽입 */
            char *hdr_end = strstr(resp, "\r\n\r\n");
            if (hdr_end && (size_t)(rlen + strlen(origin) + 64) < sizeof resp) {
                /* 뒤로 밀기 */
                size_t tail_len = rlen - (hdr_end - resp);
                memmove(hdr_end + 2 + tail_len + 62, hdr_end + 2, tail_len); /* 여유 확보 */
                int add = sprintf(hdr_end + 2,
                    "Access-Control-Allow-Origin: %s\r\n"
                    "Access-Control-Allow-Credentials: true\r\n", origin);
                rlen += add;
            }
        }
        write(client_fd, resp, rlen);
    } else if (rlen == -1) {                 /* 라우트 불일치 */
        write(client_fd,
              "HTTP/1.1 404 Not Found\r\nContent-Length:0\r\n\r\n", 48);
    } else {                                 /* -2 등 내부 오류 */
        write(client_fd,
              "HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\n\r\n",
              57);
    }

    close(client_fd);
    return NULL;
}
