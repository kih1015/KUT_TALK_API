#include "http_server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BACKLOG 10
#define BUF_SIZE 4096

static void handle_client(int client_fd) {
    char buf[BUF_SIZE];
    int len = read(client_fd, buf, BUF_SIZE - 1);
    if (len <= 0) {
        close(client_fd);
        return;
    }
    buf[len] = '\0';

    if (strncmp(buf, "GET / ", 6) == 0) {
        const char *response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 12\r\n\r\n"
            "Hello World";
        write(client_fd, response, strlen(response));
    } else if (strncmp(buf, "POST /register", 15) == 0) {
        const char *resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 20\r\n\r\n"
            "Registration handler";
        write(client_fd, resp, strlen(resp));
    } else if (strncmp(buf, "POST /login", 12) == 0) {
        const char *resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 14\r\n\r\n"
            "Login handler";
        write(client_fd, resp, strlen(resp));
    } else {
        const char *resp =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 9\r\n\r\n"
            "Not Found";
        write(client_fd, resp, strlen(resp));
    }
    close(client_fd);
}

void http_server_start(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return;
    }
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return;
    }

    printf("HTTP server listening on port %d\n", port);
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        handle_client(client_fd);
    }
    close(server_fd);
}
