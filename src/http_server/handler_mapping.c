#include "handler_mapping.h"
#include "http_server/http_request.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>

#define BUF_SIZE    4096
const struct route ROUTES[] = {
    {"POST", "/users/login", user_controller_login},
    {"POST", "/users", user_controller_register},
    {"GET", "/users/me", user_controller_get_me},
    {"POST", "/users/logout", user_controller_logout},
};
const size_t ROUTE_CNT = sizeof ROUTES / sizeof *ROUTES;

void *handle_client_thread(void *arg) {
    int fd = *(int *) arg;
    free(arg);

    char buf[BUF_SIZE];
    int n = read(fd, buf, sizeof buf - 1);
    if (n <= 0) {
        close(fd);
        return NULL;
    }
    buf[n] = '\0';

    struct http_request req;
    if (parse_http_request(buf, n, &req) != 0) {
        write(fd, "HTTP/1.1 400 Bad Request\r\nContent-Length:0\r\n\r\n", 50);
        close(fd);
        return NULL;
    }

    char resp[8192];
    int rlen = -1;
    for (size_t i = 0; i < ROUTE_CNT; ++i)
        if (!strcmp(req.method, ROUTES[i].method) &&
            !strcmp(req.path, ROUTES[i].path)) {
            rlen = ROUTES[i].handler(&req, resp, sizeof resp);
            break;
        }

    if (rlen > 0)
        write(fd, resp, rlen);
    else if (rlen == -1)
        write(fd, "HTTP/1.1 404 Not Found\r\nContent-Length:0\r\n\r\n", 48);
    else
        write(fd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\n\r\n", 57);

    close(fd);
    return NULL;
}
