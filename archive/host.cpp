// server_omp.c — minimal in-memory Yrs host with OpenMP broadcast fan-out
// Build: cc -O2 -fopenmp server_omp.c -o server -lwebsockets -lyrs
// Run:   ./server_omp 9000
//
// Functionally identical to the previous minimal server; OpenMP only speeds up
// message fan-out when many clients are connected.

#define _POSIX_C_SOURCE 200809L
#include <libwebsockets.h>
#include <omp.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// #include "yffi.h"

extern "C" {
#include "libyrs.h"
}

static volatile int running = 1;

// ----- CRDT in-mem doc -----
static YDoc *g_doc = NULL;

// ----- Peer def -----
typedef struct peer {
    struct lws *wsi;
    unsigned char *pending;
    size_t pending_len;
    omp_lock_t lock;  // per-peer lock for thread-safe access
    struct peer *next;
} peer;

static peer *peers = NULL;

// lock around peers list
static omp_lock_t peers_lock;

static void peers_add(struct lws *wsi) {
    omp_set_lock(&peers_lock);
    peer *p = (peer*)calloc(1, sizeof(peer));
    p->wsi = wsi;
    omp_init_lock(&p->lock);  // initialize per-peer lock
    p->next = peers;
    peers = p;
    omp_unset_lock(&peers_lock);
}

static void peers_remove(struct lws *wsi) {
    omp_set_lock(&peers_lock);
    peer **pp = &peers;
    while (*pp) {
        if ((*pp)->wsi == wsi) {
            peer *dead = *pp;
            *pp = (*pp)->next;
            // Clean up peer resources
            omp_set_lock(&dead->lock);  // acquire lock before cleanup
            if (dead->pending) free(dead->pending);
            omp_unset_lock(&dead->lock);
            omp_destroy_lock(&dead->lock);  // destroy per-peer lock
            free(dead);
            break;
        }
        pp = &((*pp)->next);
    }
    omp_unset_lock(&peers_lock);
}

static peer* peers_find(struct lws *wsi) {
    for (peer *p = peers; p; p = p->next)
        if (p->wsi == wsi) return p;
    return NULL;
}

static void queue_to_peer(peer *p, const unsigned char *data, size_t len) {
    if (!p) return;
    omp_set_lock(&p->lock);  // protect peer's pending data
    if (p->pending) { free(p->pending); p->pending = NULL; p->pending_len = 0; }
    p->pending = (unsigned char*)malloc(len);
    if (!p->pending) {
        omp_unset_lock(&p->lock);
        return;
    }
    memcpy(p->pending, data, len);
    p->pending_len = len;
    omp_unset_lock(&p->lock);
    lws_callback_on_writable(p->wsi);
}

// ----- Broadcast using OpenMP -----
static void broadcast_parallel(const unsigned char *data, size_t len, struct lws *exclude) {
    // snapshot peers list under lock
    omp_set_lock(&peers_lock);
    int count = 0;
    for (peer *p = peers; p; p = p->next) count++;
    peer **snapshot = (peer**)malloc(sizeof(peer*) * count);
    int i = 0;
    for (peer *p = peers; p; p = p->next) snapshot[i++] = p;
    omp_unset_lock(&peers_lock);

    // fan-out in parallel (each thread queues its own peer)
    #pragma omp parallel for schedule(static)
    for (int j = 0; j < count; ++j) {
        peer *p = snapshot[j];
        if (p->wsi != exclude)
            queue_to_peer(p, data, len);
    }
    free(snapshot);
}

// ----- WebSocket protocol -----
static int callback_crdt(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            peers_add(wsi);
            // Encode full document state as update for new client
            YTransaction *read_txn = ydoc_read_transaction(g_doc);
            uint32_t slen = 0;
            // Get full state by diffing against empty state vector
            char *state = ytransaction_state_diff_v1(read_txn, NULL, 0, &slen);
            ytransaction_commit(read_txn);
            if (state && slen > 0) {
                peer *p = peers_find(wsi);
                queue_to_peer(p, (unsigned char*)state, (size_t)slen);
                ybinary_destroy(state, slen);
            }
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            // serial CRDT mutation
            // #pragma omp critical(apply_crdt)
            // {
            YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
            ytransaction_apply(txn, (const char*)in, (uint32_t)len);
            ytransaction_commit(txn);
            // }
            // parallel broadcast of same bytes
            broadcast_parallel((unsigned char*)in, len, wsi);
            break;
        }

        case LWS_CALLBACK_SERVER_WRITEABLE: {
            peer *p = peers_find(wsi);
            if (p) {
                omp_set_lock(&p->lock);  // protect peer's pending data
                if (p->pending && p->pending_len > 0) {
                    size_t out_len = LWS_PRE + p->pending_len;
                    unsigned char *out = (unsigned char*)malloc(out_len);
                    memcpy(out + LWS_PRE, p->pending, p->pending_len);
                    size_t send_len = p->pending_len;
                    free(p->pending);
                    p->pending = NULL;
                    p->pending_len = 0;
                    omp_unset_lock(&p->lock);
                    lws_write(wsi, out + LWS_PRE, send_len, LWS_WRITE_BINARY);
                    free(out);
                } else {
                    omp_unset_lock(&p->lock);
                }
            }
            break;
        }

        case LWS_CALLBACK_CLOSED:
        case LWS_CALLBACK_WSI_DESTROY:
            peers_remove(wsi);
            break;
        default: break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "crdt-proto", callback_crdt, 0, 64 * 1024, 0, NULL, 0 },
    { NULL, NULL, 0, 0, 0, NULL, 0 } 
    // required by libwebsockets: the protocols array must end with a NULL-terminated entry.
};

static void on_signal(int sig) { 
    printf("Received signal %d, shutting down...\n", sig);
    (void)sig; 
    running = 0; 
}

int main(int argc, char **argv) {
    int port = 9000;
    if (argc >= 2) port = atoi(argv[1]);

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    omp_init_lock(&peers_lock);
    g_doc = ydoc_new();
    if (!g_doc) { fprintf(stderr, "Error: Failed to create document\n"); return 1; }

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    struct lws_context *context = lws_create_context(&info);
    if (!context) { fprintf(stderr, "Error: Failed to create WebSocket context\n"); return 1; }

    while (running) lws_service(context, 50);

    lws_context_destroy(context);
    ydoc_destroy(g_doc);
    omp_destroy_lock(&peers_lock);
    return 0;
}