#include "document.h"
#include <stdio.h>
#include <string.h>

Document::Document() : m_doc(nullptr), m_text(nullptr) {}

Document::~Document() {
    if (m_doc) {
        ydoc_destroy(m_doc);
        m_doc = nullptr;
        m_text = nullptr;
    }
}

bool Document::init(const char* shared_type_name) {
    m_doc = ydoc_new();
    if (!m_doc) {
        fprintf(stderr, "[Document] Failed to create YDoc\n");
        return false;
    }

    m_text = ytext(m_doc, shared_type_name);
    if (!m_text) {
        fprintf(stderr, "[Document] Failed to create YText with name '%s'\n", shared_type_name);
        ydoc_destroy(m_doc);
        m_doc = nullptr;
        return false;
    }

    printf("[Document] Initialized with shared type '%s'\n", shared_type_name);
    return true;
}

bool Document::apply_update(const uint8_t* update, size_t len) {
    if (!m_doc || len == 0) {
        return false;
    }

    // Try V1 format first
    YTransaction* txn = ydoc_write_transaction(m_doc, 0, nullptr);
    uint8_t err = ytransaction_apply(txn, (const char*)update, (uint32_t)len);

    if (err != 0) {
        // V1 failed, try V2
        ytransaction_commit(txn);

        txn = ydoc_write_transaction(m_doc, 0, nullptr);
        err = ytransaction_apply_v2(txn, (const char*)update, (uint32_t)len);

        if (err != 0) {
            fprintf(stderr, "[Document] Failed to apply update: V1 error=%d, V2 error=%d\n", err, err);
            ytransaction_commit(txn);
            return false;
        }
    }

    ytransaction_commit(txn);
    return true;
}

uint8_t* Document::get_state_as_update(size_t* out_len) {
    if (!m_doc) {
        *out_len = 0;
        return nullptr;
    }

    YTransaction* txn = ydoc_read_transaction(m_doc);
    uint32_t state_len = 0;
    char* state = ytransaction_state_diff_v1(txn, nullptr, 0, &state_len);
    ytransaction_commit(txn);

    if (!state || state_len == 0) {
        *out_len = 0;
        return nullptr;
    }

    // Copy to uint8_t buffer
    uint8_t* result = (uint8_t*)malloc(state_len);
    memcpy(result, state, state_len);
    *out_len = state_len;

    ybinary_destroy(state, state_len);
    return result;
}

uint8_t* Document::get_state_vector(size_t* out_len) {
    if (!m_doc) {
        *out_len = 0;
        return nullptr;
    }

    YTransaction* txn = ydoc_read_transaction(m_doc);
    uint32_t sv_len = 0;
    char* sv = ytransaction_state_vector_v1(txn, &sv_len);
    ytransaction_commit(txn);

    if (!sv || sv_len == 0) {
        *out_len = 0;
        return nullptr;
    }

    uint8_t* result = (uint8_t*)malloc(sv_len);
    memcpy(result, sv, sv_len);
    *out_len = sv_len;

    ybinary_destroy(sv, sv_len);
    return result;
}

uint8_t* Document::get_state_diff(const uint8_t* client_sv, size_t sv_len, size_t* out_len) {
    if (!m_doc) {
        *out_len = 0;
        return nullptr;
    }

    YTransaction* txn = ydoc_read_transaction(m_doc);
    uint32_t diff_len = 0;
    char* diff = ytransaction_state_diff_v1(txn, (const char*)client_sv, (uint32_t)sv_len, &diff_len);
    ytransaction_commit(txn);

    if (!diff || diff_len == 0) {
        *out_len = 0;
        return nullptr;
    }

    uint8_t* result = (uint8_t*)malloc(diff_len);
    memcpy(result, diff, diff_len);
    *out_len = diff_len;

    ybinary_destroy(diff, diff_len);
    return result;
}

char* Document::get_text_content() {
    if (!m_doc || !m_text) {
        return nullptr;
    }

    YTransaction* txn = ydoc_read_transaction(m_doc);
    const char* content = ytext_string(m_text, txn);

    char* result = nullptr;
    if (content) {
        size_t len = strlen(content);
        result = (char*)malloc(len + 1);
        memcpy(result, content, len + 1);
        ystring_destroy((char*)content);
    }

    ytransaction_commit(txn);
    return result;
}
