#include "peer.h"
#include <stdlib.h>
#include <string.h>

Peer* g_peers = nullptr;
omp_lock_t g_peers_lock;

void peers_init() {
    omp_init_lock(&g_peers_lock);
    g_peers = nullptr;
}

void peers_destroy() {
    omp_set_lock(&g_peers_lock);

    Peer* p = g_peers;
    while (p) {
        Peer* next = p->next;

        // Free pending messages
        omp_set_lock(&p->lock);
        PendingMessage* msg = p->pending_queue;
        while (msg) {
            PendingMessage* next_msg = msg->next;
            free(msg->data);
            free(msg);
            msg = next_msg;
        }
        omp_unset_lock(&p->lock);

        if (p->awareness_json) {
            free(p->awareness_json);
            p->awareness_json = nullptr;
            p->awareness_len = 0;
        }

        omp_destroy_lock(&p->lock);
        free(p);
        p = next;
    }

    g_peers = nullptr;
    omp_unset_lock(&g_peers_lock);
    omp_destroy_lock(&g_peers_lock);
}

Peer* peers_add(struct lws* wsi) {
    Peer* p = (Peer*)calloc(1, sizeof(Peer));
    p->wsi = wsi;
    p->synced = false;
    p->pending_queue = nullptr;
    p->client_id = 0;
    p->awareness_json = nullptr;
    p->awareness_len = 0;
    omp_init_lock(&p->lock);

    omp_set_lock(&g_peers_lock);
    p->next = g_peers;
    g_peers = p;
    omp_unset_lock(&g_peers_lock);

    return p;
}

void peers_remove(struct lws* wsi) {
    omp_set_lock(&g_peers_lock);

    Peer** pp = &g_peers;
    while (*pp) {
        Peer* p = *pp;
        if (p->wsi == wsi) {
            *pp = p->next;

            // Free pending messages
            omp_set_lock(&p->lock);
            PendingMessage* msg = p->pending_queue;
            while (msg) {
                PendingMessage* next_msg = msg->next;
                free(msg->data);
                free(msg);
                msg = next_msg;
            }
            omp_unset_lock(&p->lock);

            if (p->awareness_json) {
                free(p->awareness_json);
                p->awareness_json = nullptr;
                p->awareness_len = 0;
            }

            omp_destroy_lock(&p->lock);
            free(p);
            break;
        }
        pp = &p->next;
    }

    omp_unset_lock(&g_peers_lock);
}

Peer* peers_find(struct lws* wsi) {
    omp_set_lock(&g_peers_lock);

    Peer* p = g_peers;
    while (p) {
        if (p->wsi == wsi) {
            omp_unset_lock(&g_peers_lock);
            return p;
        }
        p = p->next;
    }

    omp_unset_lock(&g_peers_lock);
    return nullptr;
}

int peers_count() {
    omp_set_lock(&g_peers_lock);

    int count = 0;
    Peer* p = g_peers;
    while (p) {
        count++;
        p = p->next;
    }

    omp_unset_lock(&g_peers_lock);
    return count;
}

void peer_queue_message(Peer* p, const uint8_t* data, size_t len) {
    PendingMessage* msg = (PendingMessage*)malloc(sizeof(PendingMessage));
    msg->data = (uint8_t*)malloc(len);
    memcpy(msg->data, data, len);
    msg->len = len;
    msg->next = nullptr;

    omp_set_lock(&p->lock);

    if (!p->pending_queue) {
        p->pending_queue = msg;
    } else {
        // Append to end
        PendingMessage* tail = p->pending_queue;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = msg;
    }

    omp_unset_lock(&p->lock);

    // Request writable callback
    lws_callback_on_writable(p->wsi);
}

PendingMessage* peer_dequeue_message(Peer* p) {
    omp_set_lock(&p->lock);

    PendingMessage* msg = p->pending_queue;
    if (msg) {
        p->pending_queue = msg->next;
    }

    omp_unset_lock(&p->lock);
    return msg;
}

void peer_free_message(PendingMessage* msg) {
    if (msg) {
        free(msg->data);
        free(msg);
    }
}
