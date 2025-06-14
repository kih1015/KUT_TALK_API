#include "http_server.h"
#include <stdlib.h>

int main(int argc, char **argv) {
    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    http_server_start(port);
    return 0;
}
