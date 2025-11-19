#include "protocol.h"
#include <stdio.h>

// Decode variable-length unsigned integer (y-websocket encoding)
static size_t decode_varuint(const unsigned char *data, size_t len, uint32_t *value) {
    if (len == 0) return 0;
    
    uint32_t num = 0;
    size_t offset = 0;
    uint8_t shift = 0;
    
    while (offset < len) {
        uint8_t byte = data[offset++];
        num |= (byte & 0x7F) << shift;
        shift += 7;
        
        if ((byte & 0x80) == 0) {
            // Last byte (high bit not set)
            *value = num;
            return offset;
        }
        
        if (shift >= 32) {
            // Overflow protection
            return 0;
        }
    }
    
    return 0; // Incomplete varuint
}

uint8_t protocol_parse_message_type(const unsigned char *data, size_t len) {
    if (len == 0) return 0xFF; // Invalid
    return data[0];
}

const unsigned char* protocol_get_payload(const unsigned char *data, size_t len, size_t *payload_len) {
    if (len <= 1) {
        *payload_len = 0;
        return NULL;
    }
    
    uint8_t msg_type = data[0];
    
    // For SyncStep2 (type 1), the format is:
    // [messageType (1 byte)] [varUint length] [actual update data]
    if (msg_type == Y_SYNC_STEP2) {
        uint32_t encoded_len = 0;
        size_t varuint_bytes = decode_varuint(data + 1, len - 1, &encoded_len);
        
        if (varuint_bytes == 0) {
            fprintf(stderr, "[Protocol] Failed to decode varUint\n");
            *payload_len = 0;
            return NULL;
        }
        
        // Skip: [message type (1)] + [varUint bytes]
        size_t payload_offset = 1 + varuint_bytes;
        
        if (payload_offset >= len) {
            *payload_len = 0;
            return NULL;
        }
        
        *payload_len = len - payload_offset;
        printf("[Protocol] Decoded SyncStep2: varUint=%u bytes, offset=%zu, payload=%zu\n", 
               encoded_len, varuint_bytes, *payload_len);
        
        return data + payload_offset;
    }
    
    // For other message types, just skip the first byte
    *payload_len = len - 1;
    return data + 1;
}

bool protocol_is_crdt_update(uint8_t message_type) {
    // Only Step 2 (Y_SYNC_STEP2) contains actual CRDT updates
    // Step 1 is just a state vector request
    // Awareness is ephemeral cursor/presence data
    return message_type == Y_SYNC_STEP2;
}

