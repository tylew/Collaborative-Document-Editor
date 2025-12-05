#ifndef PEER_H
#define PEER_H

#include <libwebsockets.h>
#include <omp.h>

// Pending message to send to peer
struct PendingMessage {
    uint8_t* data;
    size_t len;
    PendingMessage* next;
};

// Peer (connected client)
struct Peer {
    struct lws* wsi;
    bool synced;           // Has received initial state?
    PendingMessage* pending_queue;
    omp_lock_t lock;
    Peer* next;
};

// Global peer list (thread-safe)
extern Peer* g_peers;
extern omp_lock_t g_peers_lock;

// Initialize peer system
void peers_init();

// Cleanup peer system
void peers_destroy();

// Add new peer
Peer* peers_add(struct lws* wsi);

// Remove peer
void peers_remove(struct lws* wsi);

// Find peer by wsi
Peer* peers_find(struct lws* wsi);

// Get peer count
int peers_count();

// Queue message for peer
void peer_queue_message(Peer* p, const uint8_t* data, size_t len);

// Dequeue next message for peer
PendingMessage* peer_dequeue_message(Peer* p);

// Free message
void peer_free_message(PendingMessage* msg);

#endif // PEER_H
