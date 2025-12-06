#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Encode variable-length unsigned integer (varint)
// Uses 7 bits per byte, 8th bit = continuation flag
size_t encode_varuint(uint32_t value, uint8_t* buffer) {
    size_t pos = 0;

    while (value >= 0x80) {
        buffer[pos++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    buffer[pos++] = (uint8_t)(value & 0x7F);

    return pos;
}

// Decode variable-length unsigned integer
// Returns bytes consumed, 0 on error
size_t decode_varuint(const uint8_t* data, size_t len, uint32_t* value) {
    if (len == 0) return 0;

    uint32_t result = 0;
    size_t pos = 0;
    uint32_t shift = 0;

    while (pos < len) {
        uint8_t byte = data[pos++];
        result |= ((uint32_t)(byte & 0x7F)) << shift;

        if ((byte & 0x80) == 0) {
            // Last byte (no continuation bit)
            *value = result;
            return pos;
        }

        shift += 7;
        if (shift >= 32) {
            // Overflow protection
            return 0;
        }
    }

    return 0; // Incomplete varint
}

// Parse message type from first byte
MessageType parse_message_type(const uint8_t* data, size_t len) {
    if (len == 0) return (MessageType)-1;

    uint8_t type = data[0];
    if (type > MSG_AWARENESS) {
        return (MessageType)-1;
    }

    return (MessageType)type;
}

// Encode SYNC_STEP1: [type=0][varuint: sv_len][state_vector]
uint8_t* encode_sync_step1(const uint8_t* state_vector, size_t sv_len, size_t* out_len) {
    // Calculate size: 1 (type) + varint + sv_len
    uint8_t varint_buf[5]; // Max varint size for uint32
    size_t varint_len = encode_varuint((uint32_t)sv_len, varint_buf);

    size_t total_len = 1 + varint_len + sv_len;
    uint8_t* buffer = (uint8_t*)malloc(total_len);

    size_t pos = 0;
    buffer[pos++] = MSG_SYNC_STEP1;
    memcpy(buffer + pos, varint_buf, varint_len);
    pos += varint_len;
    memcpy(buffer + pos, state_vector, sv_len);

    *out_len = total_len;
    return buffer;
}

// Decode SYNC_STEP1: returns pointer to state vector within data
const uint8_t* decode_sync_step1(const uint8_t* data, size_t len, size_t* sv_len) {
    if (len < 2) return NULL; // Need at least type + varint

    if (data[0] != MSG_SYNC_STEP1) {
        fprintf(stderr, "[Protocol] Expected SYNC_STEP1 (0), got %d\n", data[0]);
        return NULL;
    }

    uint32_t encoded_len = 0;
    size_t varint_bytes = decode_varuint(data + 1, len - 1, &encoded_len);

    if (varint_bytes == 0) {
        fprintf(stderr, "[Protocol] Failed to decode varint\n");
        return NULL;
    }

    size_t payload_offset = 1 + varint_bytes;
    size_t remaining = len - payload_offset;

    if (remaining < encoded_len) {
        fprintf(stderr, "[Protocol] Incomplete SYNC_STEP1: expected %u bytes, got %zu\n",
                encoded_len, remaining);
        return NULL;
    }

    *sv_len = encoded_len;
    return data + payload_offset;
}

// Encode SYNC_STEP2: [type=1][varuint: update_len][update]
uint8_t* encode_sync_step2(const uint8_t* update, size_t update_len, size_t* out_len) {
    // Calculate size: 1 (type) + varint + update_len
    uint8_t varint_buf[5];
    size_t varint_len = encode_varuint((uint32_t)update_len, varint_buf);

    size_t total_len = 1 + varint_len + update_len;
    uint8_t* buffer = (uint8_t*)malloc(total_len);

    size_t pos = 0;
    buffer[pos++] = MSG_SYNC_STEP2;
    memcpy(buffer + pos, varint_buf, varint_len);
    pos += varint_len;
    // Only copy update data if it exists
    if (update && update_len > 0) {
        memcpy(buffer + pos, update, update_len);
    }

    *out_len = total_len;
    return buffer;
}

// Decode SYNC_STEP2: returns pointer to update within data
const uint8_t* decode_sync_step2(const uint8_t* data, size_t len, size_t* update_len) {
    if (len < 2) return NULL;

    if (data[0] != MSG_SYNC_STEP2) {
        fprintf(stderr, "[Protocol] Expected SYNC_STEP2 (1), got %d\n", data[0]);
        return NULL;
    }

    uint32_t encoded_len = 0;
    size_t varint_bytes = decode_varuint(data + 1, len - 1, &encoded_len);

    if (varint_bytes == 0) {
        fprintf(stderr, "[Protocol] Failed to decode varint\n");
        return NULL;
    }

    size_t payload_offset = 1 + varint_bytes;
    size_t remaining = len - payload_offset;

    if (remaining < encoded_len) {
        fprintf(stderr, "[Protocol] Incomplete SYNC_STEP2: expected %u bytes, got %zu\n",
                encoded_len, remaining);
        return NULL;
    }

    *update_len = encoded_len;
    return data + payload_offset;
}
