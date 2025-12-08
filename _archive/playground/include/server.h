#ifndef SERVER_H
#define SERVER_H

#include <cstdint>
#include <cstddef>

// Forward declaration to avoid requiring libwebsockets in every includer
struct lws;

// Run server on specified port
int server_run(int port);

// Shutdown server
void server_shutdown();

// Broadcast message to all peers except sender
void server_broadcast(const uint8_t* data, size_t len, struct lws* exclude);

#endif // SERVER_H
