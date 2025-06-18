#include "http_request.h"
#include <string.h>
#include <ctype.h>

/* 공백·탭 스킵 */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t') ++p;
    return p;
}

/* 대소문자 무시 비교 */
static int ieq(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

int parse_http_request(char *buf, size_t len, struct http_request *out) {
    out->raw = buf;

    /* 1) 요청 라인 ------------------------------------------------- */
    char *sp1 = memchr(buf, ' ', len);
    if (!sp1) return -1;
    size_t mlen = sp1 - buf;
    if (mlen >= MAX_METHOD_LEN) return -1;
    memcpy(out->method, buf, mlen);
    out->method[mlen] = 0;

    char *sp2 = memchr(sp1 + 1, ' ', len - (sp1 + 1 - buf));
    if (!sp2) return -1;
    size_t plen = sp2 - (sp1 + 1);
    if (plen >= MAX_PATH_LEN) return -1;
    memcpy(out->path, sp1 + 1, plen);
    out->path[plen] = 0;

    /* 2) 헤더 ------------------------------------------------------ */
    char *line = sp2 + 1;
    out->header_cnt = 0;
    while (line < buf + len - 1) {
        if (*line == '\r' && line[1] == '\n') {
            out->body = line + 2; /* 바디 시작 */
            break;
        }
        char *colon = strchr(line, ':');
        if (!colon) return -1;
        char *eol = strstr(line, "\r\n");
        if (!eol) return -1;

        size_t nlen = colon - line;
        size_t vlen = eol - (colon + 1);
        if (out->header_cnt >= MAX_HEADERS) return -1;

        struct http_header *h = &out->headers[out->header_cnt++];
        memcpy(h->name, line, nlen);
        h->name[nlen] = 0;
        memcpy(h->value, skip_ws(colon + 1), vlen);
        h->value[vlen] = 0;

        line = eol + 2;
    }
    return 0;
}

/* 쿠키·임의 헤더 찾기 */
const char *http_get_header(const struct http_request *req, const char *name) {
    for (size_t i = 0; i < req->header_cnt; ++i)
        if (ieq(req->headers[i].name, name))
            return req->headers[i].value;
    return NULL;
}
