#ifndef SERVER_H
#define SERVER_H

#include <libwebsockets.h>

// Server lifecycle
int server_run(int port);
void server_shutdown();

// Broadcasting
void server_broadcast(const unsigned char *data, size_t len, struct lws *exclude);

#endif // SERVER_H

