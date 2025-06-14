#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <stddef.h>

/**
 * Starts a simple HTTP server.
 * Listens on given port, handles basic GET and POST requests.
 */
void http_server_start(int port);

#endif // HTTP_SERVER_H