#include "document.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static YDoc *g_doc = NULL;
static Branch *g_text = NULL;

void document_init() {
    g_doc = ydoc_new();
    if (!g_doc) {
        fprintf(stderr, "[Document] Failed to create YDoc\n");
        return;
    }
    
    // IMPORTANT: Must match client shared type name!
    // Client uses: ydoc.getText('document')
    g_text = ytext(g_doc, "document");
    if (!g_text) {
        fprintf(stderr, "[Document] Failed to create YText\n");
        return;
    }
    
    printf("[Document] Initialized with shared type 'document'\n");
}

void document_destroy() {
    if (g_doc) {
        ydoc_destroy(g_doc);
        g_doc = NULL;
        g_text = NULL;
    }
}

unsigned char* document_apply_update(const unsigned char *data, size_t len, size_t *out_len) {
    if (!g_doc || len == 0) {
        *out_len = 0;
        return NULL;
    }
    
    // Hex dump first 20 bytes for debugging
    printf("[Document] Raw payload hex (first %zu bytes): ", len > 20 ? 20 : len);
    for (size_t i = 0; i < len && i < 20; i++) {
        printf("%02X ", data[i]);
    }
    printf("\n");
    
    // Debug: Try to parse the update to see what format it is
    char *debug_v1 = yupdate_debug_v1((const char*)data, (uint32_t)len);
    char *debug_v2 = yupdate_debug_v2((const char*)data, (uint32_t)len);
    
    printf("[Document] Debug v1: %s\n", debug_v1 ? "valid" : "NULL");
    printf("[Document] Debug v2: %s\n", debug_v2 ? "valid" : "NULL");
    
    if (debug_v1) {
        printf("[Document] V1 content: %s\n", debug_v1);
        ystring_destroy(debug_v1);
    }
    if (debug_v2) {
        printf("[Document] V2 content: %s\n", debug_v2);
        ystring_destroy(debug_v2);
    }
    
    // Apply the update to our document
    // Try v1 first (default y-websocket encoding)
    YTransaction *txn = ydoc_write_transaction(g_doc, 0, NULL);
    uint8_t err = ytransaction_apply(txn, (const char*)data, (uint32_t)len);
    
    // If v1 fails with error 3 or 4, try v2
    if (err != 0) {
        printf("[Document] V1 apply failed (error %d), trying V2...\n", err);
        ytransaction_commit(txn);
        
        txn = ydoc_write_transaction(g_doc, 0, NULL);
        err = ytransaction_apply_v2(txn, (const char*)data, (uint32_t)len);
        
        if (err != 0) {
            fprintf(stderr, "[Document] V2 apply also failed (error %d)\n", err);
            ytransaction_commit(txn);
            *out_len = 0;
            return NULL;
        } else {
            printf("[Document] V2 apply succeeded!\n");
        }
    } else {
        printf("[Document] V1 apply succeeded!\n");
    }
    
    ytransaction_commit(txn);
    
    // Return a copy of the update to broadcast
    unsigned char *result = (unsigned char*)malloc(len);
    memcpy(result, data, len);
    *out_len = len;
    
    printf("[Document] Applied update (%zu bytes)\n", len);
    return result;
}

unsigned char* document_get_state(size_t *out_len) {
    if (!g_doc) {
        *out_len = 0;
        return NULL;
    }
    
    YTransaction *read_txn = ydoc_read_transaction(g_doc);
    uint32_t state_len = 0;
    char *state = ytransaction_state_diff_v1(read_txn, NULL, 0, &state_len);
    ytransaction_commit(read_txn);
    
    if (!state || state_len == 0) {
        *out_len = 0;
        return NULL;
    }
    
    // Copy to unsigned char buffer
    unsigned char *result = (unsigned char*)malloc(state_len);
    memcpy(result, state, state_len);
    *out_len = state_len;
    
    ybinary_destroy(state, state_len);
    return result;
}

void document_print() {
    if (!g_doc || !g_text) {
        printf("[Document] (not initialized)\n");
        return;
    }
    
    YTransaction *read_txn = ydoc_read_transaction(g_doc);
    const char *content = ytext_string(g_text, read_txn);
    
    printf("=== Document Content ===\n");
    if (content && strlen(content) > 0) {
        printf("%s\n", content);
    } else {
        printf("(empty)\n");
    }
    printf("========================\n");
    
    if (content) {
        ystring_destroy((char*)content);
    }
    ytransaction_commit(read_txn);
}

YDoc* document_get_doc() {
    return g_doc;
}

