#ifndef PEER_H
#define PEER_H

#include <libwebsockets.h>
#include <omp.h>
#include <stddef.h>

// Pending update for a peer
struct pending_update {
    unsigned char *data;
    size_t len;
    pending_update *next;
};

// Connected peer structure
struct peer {
    struct lws *wsi;
    bool synced;
    pending_update *pending;
    omp_lock_t lock;
    peer *next;
};

// Exposed globals for advanced use (e.g., broadcasting)
extern peer *g_peers;
extern omp_lock_t g_peers_lock;

// Global peer list management
void peers_init();
void peers_destroy();
peer* peers_add(struct lws *wsi);
void peers_remove(struct lws *wsi);
peer* peers_find(struct lws *wsi);
int peers_count();

// Peer update queue
void peer_queue_update(peer *p, const unsigned char *data, size_t len);
pending_update* peer_dequeue_update(peer *p);
void peer_free_update(pending_update *upd);

#endif // PEER_H

