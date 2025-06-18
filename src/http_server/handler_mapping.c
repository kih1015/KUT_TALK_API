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

void *handle_client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    int len = read(client_fd, buf, BUF_SIZE - 1);
    if (len <= 0) {
        close(client_fd);
        return NULL;
    }
    buf[len] = '\0';

    /* ------------------------------------------------------------
     * ① 요청 라인 판별
     * ---------------------------------------------------------- */
    enum { ENDPOINT_NONE, ENDPOINT_REGISTER, ENDPOINT_LOGIN } ep = ENDPOINT_NONE;

    if (strncmp(buf, "POST /users/login", 17) == 0)          /* 로그인 */
        ep = ENDPOINT_LOGIN;
    else if (strncmp(buf, "POST /users", 11) == 0)            /* 회원가입 */
        ep = ENDPOINT_REGISTER;

    if (ep == ENDPOINT_NONE) {            /* 지원하지 않는 경로 */
        const char *err =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 0\r\n\r\n";
        write(client_fd, err, strlen(err));
        close(client_fd);
        return NULL;
    }

    /* ------------------------------------------------------------
     * ② 본문 추출
     * ---------------------------------------------------------- */
    char *body = strstr(buf, "\r\n\r\n");
    if (!body) {
        const char *err =
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Length: 0\r\n\r\n";
        write(client_fd, err, strlen(err));
        close(client_fd);
        return NULL;
    }
    body += 4;                                    /* 헤더 구분자 스킵 */

    /* ------------------------------------------------------------
     * ③ 어댑터 호출 → 응답 작성
     * ---------------------------------------------------------- */
    char resp_buf[8192];
    int resp_len = -1;

    if (ep == ENDPOINT_REGISTER)
        resp_len = user_adapter_register(body, resp_buf, sizeof(resp_buf));
    else /* ENDPOINT_LOGIN */
        resp_len = user_adapter_login(body, resp_buf, sizeof(resp_buf));

    if (resp_len > 0)
        write(client_fd, resp_buf, resp_len);
    else {  /* 내부 오류 */
        const char *err =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Length: 0\r\n\r\n";
        write(client_fd, err, strlen(err));
    }

    close(client_fd);
    return NULL;
}
