#include "http_server.h"
#include "adapter/register_user_adapter.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#define BACKLOG     10
#define MAX_EVENTS  64
#define BUF_SIZE    4096

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void send_response(int client_fd, int http_code, const char *body) {
    char header[128];
    int len = body ? strlen(body) : 0;
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: %d\r\n"
        "Content-Type: application/json\r\n"
        "\r\n",
        http_code,
        (http_code == 201 ? "Created" :
         http_code == 409 ? "Conflict" :
         http_code == 507 ? "Insufficient Storage" :
         http_code == 400 ? "Bad Request" :
         "Error"),
        len
    );
    write(client_fd, header, hlen);
    if (len > 0) {
        write(client_fd, body, len);
    }
}

static void *handle_client_thread(void *arg) {
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
            // 어댑터 호출: 자료 결합도로 JSON 파싱 → 서비스 호출
            int status = register_user_adapter(body);

            switch (status) {
                case 0:
                    // 성공: Created
                    send_response(client_fd, 201, NULL);
                    break;
                case -1:
                    // 중복 ID: Conflict
                    send_response(client_fd, 409,
                        "{\"error\":\"Duplicate userid\"}");
                    break;
                case -2:
                    // 저장 공간 부족
                    send_response(client_fd, 507,
                        "{\"error\":\"Storage full\"}");
                    break;
                default:
                    // JSON 파싱 실패 등
                    send_response(client_fd, 400,
                        "{\"error\":\"Bad Request\"}");
                    break;
            }
        } else {
            // 헤더만, 바디 없음
            send_response(client_fd, 400,
                "{\"error\":\"Bad Request\"}");
        }
    } else {
        // 그 외 경로는 404
        send_response(client_fd, 404,
            "{\"error\":\"Not Found\"}");
    }

    close(client_fd);
    return NULL;
}

void http_server_start(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    set_nonblocking(server_fd);

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, BACKLOG);

    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = server_fd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev);

    printf("Listening on port %d (POST /users only)\n", port);

    while (1) {
        struct epoll_event events[MAX_EVENTS];
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == server_fd) {
                // 새 연결
                while (1) {
                    int client_fd = accept(server_fd, NULL, NULL);
                    if (client_fd < 0) break;
                    set_nonblocking(client_fd);

                    struct epoll_event cev = {
                        .events = EPOLLIN | EPOLLET,
                        .data.fd = client_fd
                    };
                    epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev);
                }
            } else if (events[i].events & EPOLLIN) {
                // 수신 데이터
                int client_fd = events[i].data.fd;
                int *pfd = malloc(sizeof(int));
                *pfd = client_fd;

                pthread_t tid;
                pthread_create(&tid, NULL, handle_client_thread, pfd);
                pthread_detach(tid);

                epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
            }
        }
    }

    close(server_fd);
    close(epfd);
}
