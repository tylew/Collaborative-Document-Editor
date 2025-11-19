#include "persistence.h"
#include <fstream>
#include <stdio.h>

static const char* PERSISTENCE_FILE = "crdt_document.bin";

bool persistence_load(YDoc *doc) {
    if (!doc) return false;
    
    std::ifstream file(PERSISTENCE_FILE, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("[Persistence] No saved document found\n");
        return false;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (size == 0) {
        printf("[Persistence] Empty file\n");
        return false;
    }
    
    char *buffer = new char[size];
    if (!file.read(buffer, size)) {
        delete[] buffer;
        return false;
    }
    
    YTransaction *txn = ydoc_write_transaction(doc, 0, NULL);
    uint8_t err = ytransaction_apply(txn, buffer, (uint32_t)size);
    ytransaction_commit(txn);
    
    delete[] buffer;
    
    if (err == 0) {
        printf("[Persistence] Loaded document (%ld bytes)\n", size);
        return true;
    } else {
        fprintf(stderr, "[Persistence] Error loading: %d\n", err);
        return false;
    }
}

bool persistence_save(YDoc *doc) {
    if (!doc) return false;
    
    YTransaction *read_txn = ydoc_read_transaction(doc);
    uint32_t state_len = 0;
    char *state = ytransaction_state_diff_v1(read_txn, NULL, 0, &state_len);
    ytransaction_commit(read_txn);
    
    if (!state || state_len == 0) {
        if (state) ybinary_destroy(state, state_len);
        return false;
    }
    
    std::ofstream file(PERSISTENCE_FILE, std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "[Persistence] Failed to open file for writing\n");
        ybinary_destroy(state, state_len);
        return false;
    }
    
    file.write(state, state_len);
    file.close();
    
    printf("[Persistence] Saved document (%u bytes)\n", state_len);
    ybinary_destroy(state, state_len);
    return true;
}

