#include "server.h"
#include "peer.h"
#include "document.h"
#include "protocol.h"
#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

static volatile int g_running = 1;
static struct lws_context* g_context = nullptr;
static Document g_document;

void signal_handler(int sig) {
    printf("\n[Server] Received signal %d, shutting down...\n", sig);
    g_running = 0;
}

void server_broadcast(const uint8_t* data, size_t len, struct lws* exclude) {
    if (len == 0) return;

    omp_set_lock(&g_peers_lock);

    int count = 0;
    Peer* p = g_peers;
    while (p) {
        if (p->wsi != exclude && p->synced) {
            peer_queue_message(p, data, len);
            count++;
        }
        p = p->next;
    }

    omp_unset_lock(&g_peers_lock);

    if (count > 0) {
        printf("[Server] Broadcast %zu bytes to %d peer(s)\n", len, count);
    }
}

static int callback_crdt(struct lws* wsi, enum lws_callback_reasons reason,
                         void* user, void* in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            printf("[Server] Client connected (total: %d)\n", peers_count() + 1);
            Peer* peer = peers_add(wsi);

            // Send full state immediately (simple approach)
            size_t state_len = 0;
            uint8_t* state = g_document.get_state_as_update(&state_len);

            if (state && state_len > 0) {
                // Encode as SYNC_STEP2
                size_t msg_len = 0;
                uint8_t* msg = encode_sync_step2(state, state_len, &msg_len);

                peer_queue_message(peer, msg, msg_len);
                peer->synced = true;

                printf("[Server] Queued initial state (%zu bytes) for new client\n", state_len);

                free(msg);
                free(state);
            } else {
                // Empty document, mark as synced anyway
                peer->synced = true;
                printf("[Server] Document empty, client synced\n");
            }

            break;
        }

        case LWS_CALLBACK_CLOSED: {
            printf("[Server] Client disconnected (remaining: %d)\n", peers_count() - 1);
            peers_remove(wsi);
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            if (len == 0) break;

            const uint8_t* data = (const uint8_t*)in;

            // Parse message type
            MessageType msg_type = parse_message_type(data, len);

            if (msg_type == MSG_SYNC_STEP1) {
                printf("[Server] Received SYNC_STEP1 (%zu bytes)\n", len);

                // Client requesting state diff
                size_t sv_len = 0;
                const uint8_t* client_sv = decode_sync_step1(data, len, &sv_len);

                if (client_sv) {
                    // Send diff based on client's state vector
                    size_t diff_len = 0;
                    uint8_t* diff = g_document.get_state_diff(client_sv, sv_len, &diff_len);

                    if (diff && diff_len > 0) {
                        size_t msg_len = 0;
                        uint8_t* msg = encode_sync_step2(diff, diff_len, &msg_len);

                        Peer* peer = peers_find(wsi);
                        if (peer) {
                            peer_queue_message(peer, msg, msg_len);
                            peer->synced = true;
                        }

                        printf("[Server] Sent state diff (%zu bytes)\n", diff_len);

                        free(msg);
                        free(diff);
                    }
                }
            }
            else if (msg_type == MSG_SYNC_STEP2) {
                printf("[Server] Received SYNC_STEP2 (%zu bytes)\n", len);

                // Client sending update
                size_t update_len = 0;
                const uint8_t* update = decode_sync_step2(data, len, &update_len);

                if (update) {
                    // Apply to document
                    if (g_document.apply_update(update, update_len)) {
                        printf("[Server] Applied update (%zu bytes)\n", update_len);

                        // Debug: print current content
                        char* content = g_document.get_text_content();
                        if (content) {
                            printf("[Server] Document content: \"%s\"\n", content);
                            free(content);
                        }

                        // Broadcast to other clients (send original encoded message)
                        server_broadcast(data, len, wsi);
                    } else {
                        fprintf(stderr, "[Server] Failed to apply update\n");
                    }
                }
            }
            else {
                fprintf(stderr, "[Server] Unknown message type: %d\n", msg_type);
            }

            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            Peer* peer = peers_find(wsi);
            if (!peer) break;

            PendingMessage* msg = peer_dequeue_message(peer);
            if (!msg) break;

            // Allocate buffer with LWS_PRE space
            uint8_t* buf = (uint8_t*)malloc(LWS_PRE + msg->len);
            memcpy(buf + LWS_PRE, msg->data, msg->len);

            int written = lws_write(wsi, buf + LWS_PRE, msg->len, LWS_WRITE_BINARY);

            if (written < 0) {
                fprintf(stderr, "[Server] Write failed\n");
            } else {
                printf("[Server] Sent %d bytes to client\n", written);
            }

            free(buf);
            peer_free_message(msg);

            // Check for more pending messages
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
        0, nullptr, 0
    },
    { nullptr, nullptr, 0, 0, 0, nullptr, 0 }
};

int server_run(int port) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize subsystems
    peers_init();

    if (!g_document.init("document")) {
        fprintf(stderr, "[Server] Failed to initialize document\n");
        return 1;
    }

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

    struct lws_vhost* vhost = lws_create_vhost(g_context, &info);
    if (!vhost) {
        fprintf(stderr, "[Server] Failed to create vhost\n");
        lws_context_destroy(g_context);
        return 1;
    }

    printf("[Server] Listening on port %d\n", port);
    printf("[Server] Shared type: 'document'\n");
    printf("[Server] Protocol: y-websocket (SYNC_STEP1/SYNC_STEP2)\n");

    // Main event loop
    while (g_running) {
        lws_service(g_context, 50);
    }

    // Cleanup
    printf("\n[Server] Shutting down...\n");

    char* content = g_document.get_text_content();
    if (content) {
        printf("[Server] Final content: \"%s\"\n", content);
        free(content);
    }

    lws_context_destroy(g_context);
    peers_destroy();

    printf("[Server] Shutdown complete\n");
    return 0;
}

void server_shutdown() {
    g_running = 0;
}
