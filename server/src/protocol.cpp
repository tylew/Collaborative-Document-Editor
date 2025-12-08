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

// Encode AWARENESS: [type=2][varuint: payload_len][payload]
// payload: [varuint client_id][varuint json_len][json bytes]
uint8_t* encode_awareness(uint32_t client_id, const char* state_json, size_t json_len, size_t* out_len) {
    // Build payload
    uint8_t cid_buf[5];
    size_t cid_len = encode_varuint(client_id, cid_buf);

    uint8_t json_len_buf[5];
    size_t json_len_var = encode_varuint((uint32_t)json_len, json_len_buf);

    size_t payload_len = cid_len + json_len_var + json_len;

    // Outer length varint
    uint8_t payload_len_buf[5];
    size_t payload_len_var = encode_varuint((uint32_t)payload_len, payload_len_buf);

    size_t total_len = 1 + payload_len_var + payload_len;
    uint8_t* buf = (uint8_t*)malloc(total_len);

    size_t pos = 0;
    buf[pos++] = MSG_AWARENESS;
    memcpy(buf + pos, payload_len_buf, payload_len_var);
    pos += payload_len_var;
    memcpy(buf + pos, cid_buf, cid_len);
    pos += cid_len;
    memcpy(buf + pos, json_len_buf, json_len_var);
    pos += json_len_var;
    if (state_json && json_len > 0) {
        memcpy(buf + pos, state_json, json_len);
        pos += json_len;
    }

    *out_len = total_len;
    return buf;
}

bool decode_awareness(const uint8_t* data, size_t len, uint32_t* client_id, char** state_json, size_t* json_len) {
    if (!data || len < 2) return false;
    if (data[0] != MSG_AWARENESS) {
        fprintf(stderr, "[Protocol] Expected AWARENESS (2), got %d\n", data[0]);
        return false;
    }

    uint32_t payload_len = 0;
    size_t payload_var = decode_varuint(data + 1, len - 1, &payload_len);
    if (payload_var == 0) {
        fprintf(stderr, "[Protocol] Failed to decode awareness payload length\n");
        return false;
    }

    size_t payload_offset = 1 + payload_var;
    if (payload_offset + payload_len > len) {
        fprintf(stderr, "[Protocol] Incomplete awareness payload\n");
        return false;
    }

    const uint8_t* payload = data + payload_offset;
    size_t remaining = payload_len;

    // client_id
    uint32_t cid = 0;
    size_t cid_bytes = decode_varuint(payload, remaining, &cid);
    if (cid_bytes == 0) {
        fprintf(stderr, "[Protocol] Failed to decode client_id in awareness\n");
        return false;
    }
    payload += cid_bytes;
    remaining -= cid_bytes;

    // json length
    uint32_t jlen = 0;
    size_t jlen_bytes = decode_varuint(payload, remaining, &jlen);
    if (jlen_bytes == 0) {
        fprintf(stderr, "[Protocol] Failed to decode json_len in awareness\n");
        return false;
    }
    payload += jlen_bytes;
    remaining -= jlen_bytes;

    if (remaining < jlen) {
        fprintf(stderr, "[Protocol] Incomplete awareness json payload\n");
        return false;
    }

    *client_id = cid;
    *json_len = jlen;

    if (jlen > 0) {
        *state_json = (char*)malloc(jlen + 1);
        memcpy(*state_json, payload, jlen);
        (*state_json)[jlen] = '\0';
    } else {
        *state_json = NULL; // indicates removal
    }

    return true;
}
