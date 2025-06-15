#include "http_server.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include "handler_mapping.h"
#include "adapter/register_user_adapter.h"

#define BACKLOG     10
#define MAX_EVENTS  64
#define BUF_SIZE    4096

static void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
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
}
