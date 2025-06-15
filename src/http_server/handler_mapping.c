#include "adapter/register_user_adapter.h"
#include "handler_mapping.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define BUF_SIZE    4096

void *handle_client_thread(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buf[BUF_SIZE];
    int len = read(client_fd, buf, BUF_SIZE - 1);
    if (len <= 0) {
        close(client_fd);
        return NULL;
    }
    buf[len] = '\0';

    // POST /users 요청만 처리
    if (strncmp(buf, "POST /users", 11) == 0) {
        char *body = strstr(buf, "\r\n\r\n");
        if (body) {
            body += 4;

            // 1) 스택 버퍼로 충분히 크게 선언 (예: 8KB)
            char resp_buf[8192];
            int len = register_user_adapter(body, resp_buf, sizeof(resp_buf));
            if (len > 0) {
                // 2) 한 번에 전송
                write(client_fd, resp_buf, len);
            } else {
                // 내부 에러
                const char *err =
                  "HTTP/1.1 500 Internal Server Error\r\n"
                  "Content-Length: 0\r\n"
                  "\r\n";
                write(client_fd, err, strlen(err));
            }
        } else {
            // 헤더만, 바디 없음
            const char *err =
              "HTTP/1.1 400 Bad Request\r\n"
              "Content-Length: 0\r\n"
              "\r\n";
            write(client_fd, err, strlen(err));
        }
    }

    close(client_fd);
    return NULL;
}