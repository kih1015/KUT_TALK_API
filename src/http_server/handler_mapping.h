#pragma once
#include <stddef.h>

void *handle_client_thread(void *arg);
typedef int (*adapter_fn)(const char *body, char *resp, size_t sz);

struct route {
    const char *method;      /* "GET" / "POST" … */
    const char *path;        /* 고정 경로 또는 접두사(prefix) */
    int  prefix;             /* 0=정확히 일치, 1=path가 접두사이면 매칭 */
    adapter_fn  handler;     /* 호출할 어댑터 */
};

/* 모든 엔드포인트를 한 곳에 선언 */
extern const struct route ROUTES[];
extern const size_t ROUTE_COUNT;