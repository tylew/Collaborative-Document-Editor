#ifndef DOCUMENT_H
#define DOCUMENT_H

extern "C" {
#include <libyrs.h>
}

// Document management
void document_init();
void document_destroy();

// Apply update from client and get the update to broadcast
// Returns malloc'd buffer that caller must free, or NULL if error
unsigned char* document_apply_update(const unsigned char *data, size_t len, size_t *out_len);

// Get current document state for new clients
unsigned char* document_get_state(size_t *out_len);

// Print current document content
void document_print();

// Get the YDoc for direct access if needed
YDoc* document_get_doc();

#endif // DOCUMENT_H

