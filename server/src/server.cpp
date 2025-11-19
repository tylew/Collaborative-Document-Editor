#include "server.h"
#include "peer.h"
#include "document.h"
#include "persistence.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <omp.h>

static volatile int g_running = 1;
static struct lws_context *g_context = NULL;

void signal_handler(int sig) {
    printf("\n[Server] Received signal %d, shutting down...\n", sig);
    g_running = 0;
}

// Helper to get all peers as array (caller must free)
static peer** get_peers_snapshot(int *count_out) {
    omp_set_lock(&g_peers_lock);
    
    // Count peers
    int count = 0;
    peer *p = g_peers;
    while (p) {
        count++;
        p = p->next;
    }
    
    if (count == 0) {
        omp_unset_lock(&g_peers_lock);
        *count_out = 0;
        return NULL;
    }
    
    // Create snapshot array
    peer **snapshot = (peer**)malloc(sizeof(peer*) * count);
    p = g_peers;
    for (int i = 0; i < count; i++) {
        snapshot[i] = p;
        p = p->next;
    }
    
    omp_unset_lock(&g_peers_lock);
    *count_out = count;
    return snapshot;
}

void server_broadcast(const unsigned char *data, size_t len, struct lws *exclude) {
    if (len == 0) return;
    
    int count = 0;
    peer **snapshot = get_peers_snapshot(&count);
    if (!snapshot) return;
    
    int broadcast_count = 0;
    
    // Broadcast to all peers except exclude
    for (int i = 0; i < count; i++) {
        peer *p = snapshot[i];
        if (p->wsi != exclude && p->synced) {
            peer_queue_update(p, data, len);
            broadcast_count++;
        }
    }
    
    free(snapshot);
    printf("[Server] Broadcast %zu bytes to %d peer(s)\n", len, broadcast_count);
}

static int callback_crdt(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            printf("[Server] Client connected\n");
            peer *p = peers_add(wsi);
            
            // Send current document state to new client
            size_t state_len = 0;
            unsigned char *state = document_get_state(&state_len);
            if (state && state_len > 0) {
                peer_queue_update(p, state, state_len);
                p->synced = true;
                printf("[Server] Queued initial state (%zu bytes) for new client\n", state_len);
                free(state);
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED: {
            printf("[Server] Client disconnected\n");
            peers_remove(wsi);
            break;
        }
        
        case LWS_CALLBACK_RECEIVE: {
            if (len == 0) break;
            
            printf("[Server] Received raw update (%zu bytes)\n", len);
            
            // Apply raw Yjs update to server document
            size_t broadcast_len = 0;
            unsigned char *broadcast_data = document_apply_update((unsigned char*)in, len, &broadcast_len);
            
            if (broadcast_data && broadcast_len > 0) {
                // Save document state to disk
                persistence_save(document_get_doc());
                
                // Broadcast to other clients
                server_broadcast(broadcast_data, broadcast_len, wsi);
                free(broadcast_data);
            }
            
            break;
        }
        
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            peer *p = peers_find(wsi);
            if (!p) break;
            
            pending_update *upd = peer_dequeue_update(p);
            if (!upd) break;
            
            unsigned char buf[LWS_PRE + upd->len];
            memcpy(&buf[LWS_PRE], upd->data, upd->len);
            
            int written = lws_write(wsi, &buf[LWS_PRE], upd->len, LWS_WRITE_BINARY);
            
            if (written < 0) {
                fprintf(stderr, "[Server] Write failed\n");
            } else {
                printf("[Server] Sent %d bytes to client\n", written);
            }
            
            peer_free_update(upd);
            
            // Check for more pending updates
            if (peers_find(wsi)) {
                lws_callback_on_writable(wsi);
            }
            
            break;
        }
        
        default:
            break;
    }
    
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "crdt-protocol",
        callback_crdt,
        0,
        4096,
        0, NULL, 0
    },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

int server_run(int port) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize subsystems
    peers_init();
    document_init();
    
    // Load persisted document
    persistence_load(document_get_doc());
    document_print();
    
    // Create WebSocket context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 | LWS_SERVER_OPTION_EXPLICIT_VHOSTS;
    
    g_context = lws_create_context(&info);
    if (!g_context) {
        fprintf(stderr, "[Server] Failed to create context\n");
        return 1;
    }
    
    struct lws_vhost *vhost = lws_create_vhost(g_context, &info);
    if (!vhost) {
        fprintf(stderr, "[Server] Failed to create vhost\n");
        lws_context_destroy(g_context);
        return 1;
    }
    
    printf("[Server] Listening on port %d\n", port);
    
    // Main event loop
    while (g_running) {
        lws_service(g_context, 50);
    }
    
    // Cleanup
    printf("\n[Server] Shutting down...\n");
    document_print();
    
    lws_context_destroy(g_context);
    document_destroy();
    peers_destroy();
    
    printf("[Server] Shutdown complete\n");
    return 0;
}

void server_shutdown() {
    g_running = 0;
}

