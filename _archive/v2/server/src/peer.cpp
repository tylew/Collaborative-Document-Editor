#include "peer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global peer list (exposed for advanced use)
peer *g_peers = NULL;
omp_lock_t g_peers_lock;

void peers_init() {
    omp_init_lock(&g_peers_lock);
    g_peers = NULL;
}

void peers_destroy() {
    omp_set_lock(&g_peers_lock);
    peer *p = g_peers;
    while (p) {
        peer *next = p->next;
        
        // Free pending updates
        pending_update *upd = p->pending;
        while (upd) {
            pending_update *next_upd = upd->next;
            free(upd->data);
            free(upd);
            upd = next_upd;
        }
        
        omp_destroy_lock(&p->lock);
        free(p);
        p = next;
    }
    g_peers = NULL;
    omp_unset_lock(&g_peers_lock);
    omp_destroy_lock(&g_peers_lock);
}

peer* peers_add(struct lws *wsi) {
    peer *p = (peer*)calloc(1, sizeof(peer));
    p->wsi = wsi;
    p->synced = false;
    p->pending = NULL;
    omp_init_lock(&p->lock);
    
    omp_set_lock(&g_peers_lock);
    p->next = g_peers;
    g_peers = p;
    omp_unset_lock(&g_peers_lock);
    
    return p;
}

void peers_remove(struct lws *wsi) {
    omp_set_lock(&g_peers_lock);
    peer **pp = &g_peers;
    while (*pp) {
        peer *p = *pp;
        if (p->wsi == wsi) {
            *pp = p->next;
            
            // Free pending updates
            omp_set_lock(&p->lock);
            pending_update *upd = p->pending;
            while (upd) {
                pending_update *next_upd = upd->next;
                free(upd->data);
                free(upd);
                upd = next_upd;
            }
            omp_unset_lock(&p->lock);
            
            omp_destroy_lock(&p->lock);
            free(p);
            break;
        }
        pp = &p->next;
    }
    omp_unset_lock(&g_peers_lock);
}

peer* peers_find(struct lws *wsi) {
    omp_set_lock(&g_peers_lock);
    peer *p = g_peers;
    while (p) {
        if (p->wsi == wsi) {
            omp_unset_lock(&g_peers_lock);
            return p;
        }
        p = p->next;
    }
    omp_unset_lock(&g_peers_lock);
    return NULL;
}

int peers_count() {
    omp_set_lock(&g_peers_lock);
    int count = 0;
    peer *p = g_peers;
    while (p) {
        count++;
        p = p->next;
    }
    omp_unset_lock(&g_peers_lock);
    return count;
}

void peer_queue_update(peer *p, const unsigned char *data, size_t len) {
    pending_update *upd = (pending_update*)malloc(sizeof(pending_update));
    upd->data = (unsigned char*)malloc(len);
    memcpy(upd->data, data, len);
    upd->len = len;
    upd->next = NULL;
    
    omp_set_lock(&p->lock);
    if (!p->pending) {
        p->pending = upd;
    } else {
        pending_update *tail = p->pending;
        while (tail->next) tail = tail->next;
        tail->next = upd;
    }
    omp_unset_lock(&p->lock);
    
    lws_callback_on_writable(p->wsi);
}

pending_update* peer_dequeue_update(peer *p) {
    omp_set_lock(&p->lock);
    pending_update *upd = p->pending;
    if (upd) {
        p->pending = upd->next;
    }
    omp_unset_lock(&p->lock);
    return upd;
}

void peer_free_update(pending_update *upd) {
    if (upd) {
        free(upd->data);
        free(upd);
    }
}

