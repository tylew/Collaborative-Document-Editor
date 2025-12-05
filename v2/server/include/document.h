#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <stddef.h>
#include <stdint.h>

extern "C" {
#include <libyrs.h>
}

class Document {
public:
    Document();
    ~Document();

    // Initialize document with shared type name
    bool init(const char* shared_type_name);

    // Apply update from client
    bool apply_update(const uint8_t* update, size_t len);

    // Get full state as update (for new clients)
    uint8_t* get_state_as_update(size_t* out_len);

    // Get state vector (what we have)
    uint8_t* get_state_vector(size_t* out_len);

    // Get state diff based on client's state vector
    uint8_t* get_state_diff(const uint8_t* client_sv, size_t sv_len, size_t* out_len);

    // Get current text content (for debugging)
    char* get_text_content();

    // Get underlying YDoc (for persistence)
    YDoc* get_doc() { return m_doc; }

private:
    YDoc* m_doc;
    Branch* m_text;
};

#endif // DOCUMENT_H
