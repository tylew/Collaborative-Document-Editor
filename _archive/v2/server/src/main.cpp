#include "server.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port: %d\n", port);
        return 1;
    }
    
    printf("===========================================\n");
    printf("   CRDT Collaborative Document Server\n");
    printf("===========================================\n\n");
    
    return server_run(port);
}

