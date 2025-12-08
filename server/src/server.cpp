#include "server.h"
#include "peer.h"
#include "document.h"
#include "protocol.h"
#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

static volatile int g_running = 1;
static struct lws_context* g_context = nullptr;
static Document g_document;

// Helper to duplicate JSON strings safely
static char* dup_json(const char* src, size_t len) {
    if (!src || len == 0) return nullptr;
    char* out = (char*)malloc(len + 1);
    memcpy(out, src, len);
    out[len] = '\0';
    return out;
}

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

            // Don't send state immediately - wait for client's SYNC_STEP1 for proper differential sync
            // This eliminates race conditions between initial sync and concurrent updates
            peer->synced = false;

            // Send existing awareness states to the new peer
            omp_set_lock(&g_peers_lock);
            Peer* p = g_peers;
            while (p) {
                if (p != peer && p->client_id != 0 && p->awareness_json && p->awareness_len > 0) {
                    size_t msg_len = 0;
                    uint8_t* msg = encode_awareness(p->client_id, p->awareness_json, p->awareness_len, &msg_len);
                    if (msg && msg_len > 0) {
                        peer_queue_message(peer, msg, msg_len);
                        free(msg);
                    }
                }
                p = p->next;
            }
            omp_unset_lock(&g_peers_lock);

            break;
        }

        case LWS_CALLBACK_CLOSED: {
            printf("[Server] Client disconnected (remaining: %d)\n", peers_count() - 1);

            // Broadcast awareness removal if client_id known
            Peer* peer = peers_find(wsi);
            if (peer && peer->client_id != 0) {
                size_t msg_len = 0;
                uint8_t* msg = encode_awareness(peer->client_id, nullptr, 0, &msg_len);
                if (msg && msg_len > 0) {
                    omp_set_lock(&g_peers_lock);
                    Peer* p = g_peers;
                    while (p) {
                        if (p->wsi != wsi) {
                            peer_queue_message(p, msg, msg_len);
                        }
                        p = p->next;
                    }
                    omp_unset_lock(&g_peers_lock);
                    free(msg);
                }
            }

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
                // Log first few bytes for debugging
                printf("[Server] SYNC_STEP1 bytes:");
                for (size_t i = 0; i < len && i < 16; i++) {
                    printf(" %02x", (unsigned char)data[i]);
                }
                printf("\n");

                // Send proper initial state from Yrs
                size_t state_len = 0;
                uint8_t* state = g_document.get_state_as_update(&state_len);

                size_t msg_len = 0;
                uint8_t* msg = encode_sync_step2(state, state_len, &msg_len);

                Peer* peer = peers_find(wsi);
                if (peer) {
                    peer_queue_message(peer, msg, msg_len);
                    peer->synced = true;
                }

                printf("[Server] Sent initial state (%zu bytes) as SYNC_STEP2\n", state_len);

                free(msg);
                if (state) free(state);
            }
            else if (msg_type == MSG_SYNC_STEP2) {
                printf("[Server] Received SYNC_STEP2 (%zu bytes)\n", len);

                // Client sending update - try to decode and apply
                size_t update_len = 0;
                const uint8_t* update = decode_sync_step2(data, len, &update_len);

                if (update && update_len > 0) {
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
                } else {
                    fprintf(stderr, "[Server] Failed to decode SYNC_STEP2 message (%zu bytes)\n", len);
                    // Log first few bytes for debugging
                    fprintf(stderr, "[Server] Message bytes:");
                    for (size_t i = 0; i < len && i < 16; i++) {
                        fprintf(stderr, " %02x", data[i]);
                    }
                    fprintf(stderr, "\n");
                }
            }
            else if (msg_type == MSG_AWARENESS) {
                uint32_t client_id = 0;
                char* state_json = nullptr;
                size_t json_len = 0;

                if (decode_awareness(data, len, &client_id, &state_json, &json_len)) {
                    Peer* peer = peers_find(wsi);
                    if (peer) {
                        peer->client_id = client_id;

                        // Replace stored awareness
                        if (peer->awareness_json) {
                            free(peer->awareness_json);
                            peer->awareness_json = nullptr;
                            peer->awareness_len = 0;
                        }
                        if (json_len > 0 && state_json) {
                            peer->awareness_json = state_json;
                            peer->awareness_len = json_len;
                            printf("[Server] Awareness update from client %u: %.*s\n",
                                   client_id, (int)json_len, peer->awareness_json);
                        } else {
                            // Removal
                            printf("[Server] Awareness removal for client %u\n", client_id);
                            if (state_json) free(state_json);
                        }

                        // Broadcast to other peers (awareness is independent of sync status)
                        omp_set_lock(&g_peers_lock);
                        Peer* p = g_peers;
                        while (p) {
                            if (p->wsi != wsi) {
                                peer_queue_message(p, data, len);
                            }
                            p = p->next;
                        }
                        omp_unset_lock(&g_peers_lock);
                    } else {
                        if (state_json) free(state_json);
                    }
                } else {
                    fprintf(stderr, "[Server] Failed to decode AWARENESS message\n");
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

    if (!g_document.init("quill")) {
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
    printf("[Server] Shared type: 'quill' (matches y-quill client)\n");
    printf("[Server] Protocol: y-websocket (SYNC_STEP1/STEP2)\n");

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
