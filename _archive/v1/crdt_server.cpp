// crdt_server.cpp — CRDT Document Server with WebSocket synchronization
// Build: g++ -O2 -fopenmp crdt_server.cpp -o crdt_server -lwebsockets -lyrs -pthread
// Run:   ./crdt_server [port]
//
// This server maintains a single master CRDT document and synchronizes it
// across multiple connected clients using WebSocket. All updates are
// automatically broadcast to all peers via the Yrs update observer.

#define _POSIX_C_SOURCE 200809L
#include <libwebsockets.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fstream>

extern "C" {
#include "libyrs.h"
}

static volatile int running = 1;

// ----- Master CRDT document -----
static YDoc *g_master_doc = NULL;
static Branch *g_text = NULL;  // Shared text document

// ----- Peer management -----
typedef struct peer {
    struct lws *wsi;
    unsigned char *pending;
    size_t pending_len;
    omp_lock_t lock;
    struct peer *next;
    bool synced;  // Has received initial state
} peer;

static peer *g_peers = NULL;
static omp_lock_t g_peers_lock;

// Flag to prevent broadcast loops
static bool g_applying_remote_update = false;

// ----- Peer list operations -----
static void peer_add(struct lws *wsi) {
    omp_set_lock(&g_peers_lock);
    peer *p = (peer*)calloc(1, sizeof(peer));
    p->wsi = wsi;
    p->synced = false;
    omp_init_lock(&p->lock);
    p->next = g_peers;
    g_peers = p;
    omp_unset_lock(&g_peers_lock);
    printf("[Server] Client connected\n");
}

static void peer_remove(struct lws *wsi) {
    omp_set_lock(&g_peers_lock);
    peer **pp = &g_peers;
    while (*pp) {
        if ((*pp)->wsi == wsi) {
            peer *dead = *pp;
            *pp = (*pp)->next;
            omp_set_lock(&dead->lock);
            if (dead->pending) free(dead->pending);
            omp_unset_lock(&dead->lock);
            omp_destroy_lock(&dead->lock);
            free(dead);
            printf("[Server] Client disconnected\n");
            break;
        }
        pp = &((*pp)->next);
    }
    omp_unset_lock(&g_peers_lock);
}

static peer* peer_find(struct lws *wsi) {
    omp_set_lock(&g_peers_lock);
    for (peer *p = g_peers; p; p = p->next) {
        if (p->wsi == wsi) {
            omp_unset_lock(&g_peers_lock);
            return p;
        }
    }
    omp_unset_lock(&g_peers_lock);
    return NULL;
}

static void peer_queue_update(peer *p, const unsigned char *data, size_t len) {
    if (!p) return;
    omp_set_lock(&p->lock);
    
    // Replace any pending data
    if (p->pending) {
        free(p->pending);
        p->pending = NULL;
        p->pending_len = 0;
    }
    
    p->pending = (unsigned char*)malloc(len);
    if (!p->pending) {
        omp_unset_lock(&p->lock);
        fprintf(stderr, "[Server] Failed to allocate pending buffer\n");
        return;
    }
    
    memcpy(p->pending, data, len);
    p->pending_len = len;
    omp_unset_lock(&p->lock);
    
    lws_callback_on_writable(p->wsi);
}

// ----- Broadcast updates to all connected peers -----
static void broadcast_update_parallel(const unsigned char *data, size_t len, struct lws *exclude) {
    // Take snapshot of peers under lock
    omp_set_lock(&g_peers_lock);
    int count = 0;
    for (peer *p = g_peers; p; p = p->next) {
        if (p->synced) count++;  // Only broadcast to synced clients
    }
    
    if (count == 0) {
        omp_unset_lock(&g_peers_lock);
        return;
    }
    
    peer **snapshot = (peer**)malloc(sizeof(peer*) * count);
    int i = 0;
    for (peer *p = g_peers; p; p = p->next) {
        if (p->synced) snapshot[i++] = p;
    }
    omp_unset_lock(&g_peers_lock);
    
    // Parallel broadcast using tasks for better load balancing
    // Tasks handle lock contention better than static scheduling
    //     Variable lock wait times: Some peers may have their locks held by the libwebsockets thread
    //     Tasks excel: Other threads continue with different peers while one waits
    //     Better throughput: More peers updated per unit time
    #pragma omp parallel
    {
        #pragma omp single
        {
            for (int j = 0; j < count; ++j) {
                peer *p = snapshot[j];
                if (p->wsi != exclude) {
                    // Create a task for each peer update
                    // This allows dynamic load balancing when lock contention varies
                    #pragma omp task firstprivate(p, data, len)
                    {
                        peer_queue_update(p, data, len);
                    }
                }
            }
            // taskwait ensures all tasks complete before we free the snapshot
            #pragma omp taskwait
        }
    }
    
    free(snapshot);
    printf("[Server] Broadcasted %zu bytes to %d client(s)\n", len, exclude ? count - 1 : count);
}

// Persistence file path
static const char* PERSISTENCE_FILE = "crdt_document.bin";

// Save document state to disk
void save_document_state() {
    YTransaction *read_txn = ydoc_read_transaction(g_master_doc);
    uint32_t state_len = 0;
    char *state = ytransaction_state_diff_v1(read_txn, NULL, 0, &state_len);
    ytransaction_commit(read_txn);
    
    if (state && state_len > 0) {
        std::ofstream file(PERSISTENCE_FILE, std::ios::binary);
        if (file.is_open()) {
            file.write(state, state_len);
            file.close();
            printf("[Server] Document saved to disk (%u bytes)\n", state_len);
        } else {
            fprintf(stderr, "[Server] Failed to open file for writing\n");
        }
        ybinary_destroy(state, state_len);
    }
}

// Load document state from disk
bool load_document_state() {
    std::ifstream file(PERSISTENCE_FILE, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("[Server] No previous document found (starting fresh)\n");
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    char* buffer = new char[size];
    if (file.read(buffer, size)) {
        YTransaction *txn = ydoc_write_transaction(g_master_doc, 0, NULL);
        uint8_t err = ytransaction_apply(txn, buffer, (uint32_t)size);
        ytransaction_commit(txn);
        
        if (err == 0) {
            printf("[Server] Document loaded from disk (%ld bytes)\n", size);
            delete[] buffer;
            return true;
        } else {
            fprintf(stderr, "[Server] Error loading document: %d\n", err);
        }
    }
    
    delete[] buffer;
    return false;
}

// ----- Document update observer callback -----
// This is called whenever the master document is modified
void doc_update_observer(void* state, uint32_t len, const char* update) {
    // Don't broadcast updates that came from a remote client
    // (they've already been broadcast in the RECEIVE callback)
    if (g_applying_remote_update) return;
    
    printf("[Server] Document updated locally (%u bytes)\n", len);
    broadcast_update_parallel((const unsigned char*)update, len, NULL);
}

// ----- WebSocket protocol callback -----
static int callback_crdt(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            peer_add(wsi);
            
            // Send full document state to new client
            YTransaction *read_txn = ydoc_read_transaction(g_master_doc);
            uint32_t state_len = 0;
            
            // Get full state as update (diff against empty state vector)
            char *state = ytransaction_state_diff_v1(read_txn, NULL, 0, &state_len);
            ytransaction_commit(read_txn);
            
            if (state && state_len > 0) {
                peer *p = peer_find(wsi);
                if (p) {
                    peer_queue_update(p, (unsigned char*)state, (size_t)state_len);
                    p->synced = true;
                    printf("[Server] Sent initial state (%u bytes) to new client\n", state_len);
                }
                ybinary_destroy(state, state_len);
            }
            break;
        }
        
        case LWS_CALLBACK_RECEIVE: {
            if (len == 0) break;
            
            // y-websocket protocol: [type (1 byte)] [payload...]
            // 0: Sync Step 1 (state vector) - relay only
            // 1: Sync Step 2 / Update (actual CRDT update) - apply AND relay  
            // 2: Awareness (cursors) - relay only
            
            uint8_t message_type = ((uint8_t*)in)[0];
            
            if (message_type == 2) {
                // Awareness: just relay (ephemeral, no persistence needed)
                printf("[Server] Awareness update - relaying\n");
                broadcast_update_parallel((unsigned char*)in, len, wsi);
                
            } else if (message_type == 1 && len > 1) {
                // Type 1: Document update - this contains actual CRDT changes
                // Apply to server's document for persistence
                const char* update_payload = (const char*)in + 1;
                uint32_t update_len = (uint32_t)(len - 1);
                
                printf("[Server] Document update (%u bytes) - applying to server\n", update_len);
                
                YTransaction *txn = ydoc_write_transaction(g_master_doc, 0, NULL);
                uint8_t err = ytransaction_apply(txn, update_payload, update_len);
                ytransaction_commit(txn);
                
                if (err == 0) {
                    printf("[Server] ✓ Update applied to server document\n");
                    // Save to disk for persistence across server restarts
                    save_document_state();
                } else {
                    // Even if apply fails, relay the message (client-to-client sync)
                    printf("[Server] Note: ytransaction_apply returned %d (may be sync protocol)\n", err);
                }
                
                // Always relay to other clients
                broadcast_update_parallel((unsigned char*)in, len, wsi);
                
            } else {
                // Type 0 or other: just relay
                printf("[Server] Sync message type %d - relaying\n", message_type);
                broadcast_update_parallel((unsigned char*)in, len, wsi);
            }
            
            break;
        }
        
        case LWS_CALLBACK_SERVER_WRITEABLE: {
            peer *p = peer_find(wsi);
            if (p) {
                omp_set_lock(&p->lock);
                
                if (p->pending && p->pending_len > 0) {
                    size_t buf_len = LWS_PRE + p->pending_len;
                    unsigned char *buf = (unsigned char*)malloc(buf_len);
                    
                    if (buf) {
                        memcpy(buf + LWS_PRE, p->pending, p->pending_len);
                        size_t send_len = p->pending_len;
                        
                        free(p->pending);
                        p->pending = NULL;
                        p->pending_len = 0;
                        
                        omp_unset_lock(&p->lock);
                        
                        lws_write(wsi, buf + LWS_PRE, send_len, LWS_WRITE_BINARY);
                        free(buf);
                    } else {
                        omp_unset_lock(&p->lock);
                        fprintf(stderr, "[Server] Failed to allocate write buffer\n");
                    }
                } else {
                    omp_unset_lock(&p->lock);
                }
            }
            break;
        }
        
        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_WSI_DESTROY:
            peer_remove(wsi);
            break;
            
        default:
            break;
    }
    
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "crdt-proto", callback_crdt, 0, 64 * 1024, 0, NULL, 0 },
    { NULL, NULL, 0, 0, 0, NULL, 0 }
};

static void on_signal(int sig) {
    printf("\n[Server] Received signal %d, shutting down...\n", sig);
    (void)sig;
    running = 0;
}

void print_document_content() {
    if (!g_text) return;
    
    YTransaction *read_txn = ydoc_read_transaction(g_master_doc);
    char *content = ytext_string(g_text, read_txn);
    
    printf("\n[Server] === Document Content ===\n");
    if (content && strlen(content) > 0) {
        printf("%s\n", content);
    } else {
        printf("(empty document)\n");
    }
    printf("=================================\n\n");
    
    ystring_destroy(content);
    ytransaction_commit(read_txn);
}

int main(int argc, char **argv) {
    int port = 9000;
    if (argc >= 2) port = atoi(argv[1]);
    
    printf("[Server] Starting CRDT Document Server on port %d\n", port);
    
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    
    // Initialize locks
    omp_init_lock(&g_peers_lock);
    
    // Create master document with shared text
    g_master_doc = ydoc_new();
    if (!g_master_doc) {
        fprintf(stderr, "[Server] Failed to create document\n");
        return 1;
    }
    
    // Get shared text object
    g_text = ytext(g_master_doc, "document");
    
    // Load existing document from disk if available
    load_document_state();
    
    // Set up document observer to monitor changes
    // YSubscription *doc_observer = ydoc_observe_updates_v1(g_master_doc, NULL, doc_update_observer);
    
    printf("[Server] Master document initialized\n");
    print_document_content();
    
    // Create WebSocket context
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    
    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "[Server] Failed to create WebSocket context\n");
        ydoc_destroy(g_master_doc);
        return 1;
    }
    
    printf("[Server] WebSocket server listening on port %d\n", port);
    printf("[Server] Press Ctrl+C to stop\n\n");
    
    // Main event loop
    int last_print = 0;
    while (running) {
        lws_service(context, 50);
        
        // Periodically print document content (every ~10 seconds)
        if (++last_print > 200) {
            // print_document_content();  // Uncomment to see periodic updates
            last_print = 0;
        }
    }
    
    printf("\n[Server] Shutting down...\n");
    print_document_content();
    
    // Clean up
    // yunobserve(doc_observer);
    lws_context_destroy(context);
    ydoc_destroy(g_master_doc);
    omp_destroy_lock(&g_peers_lock);
    
    printf("[Server] Shutdown complete\n");
    return 0;
}

