#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    int port = 9000;

    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [port]\n", argv[0]);
            return 1;
        }
    }

    printf("========================================\n");
    printf("CRDT WebSocket Server v2\n");
    printf("========================================\n");
    printf("Starting server on port %d...\n", port);

    int result = server_run(port);

    printf("========================================\n");
    return result;
}
