#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// y-websocket protocol message types
enum MessageType {
    MSG_SYNC_STEP1 = 0,  // State vector exchange
    MSG_SYNC_STEP2 = 1,  // Update data
    MSG_AWARENESS = 2    // Awareness (cursors, presence) - not implemented in v1
};

// Encode varint (variable-length unsigned integer)
size_t encode_varuint(uint32_t value, uint8_t* buffer);

// Decode varint, returns bytes consumed (0 on error)
size_t decode_varuint(const uint8_t* data, size_t len, uint32_t* value);

// Parse message type from raw data
MessageType parse_message_type(const uint8_t* data, size_t len);

// Encode SYNC_STEP1 message (state vector request/response)
// Returns allocated buffer (caller must free), sets out_len
uint8_t* encode_sync_step1(const uint8_t* state_vector, size_t sv_len, size_t* out_len);

// Decode SYNC_STEP1 message
// Returns pointer to state vector within data (no allocation), sets sv_len
const uint8_t* decode_sync_step1(const uint8_t* data, size_t len, size_t* sv_len);

// Encode SYNC_STEP2 message (update)
// Returns allocated buffer (caller must free), sets out_len
uint8_t* encode_sync_step2(const uint8_t* update, size_t update_len, size_t* out_len);

// Decode SYNC_STEP2 message
// Returns pointer to update within data (no allocation), sets update_len
const uint8_t* decode_sync_step2(const uint8_t* data, size_t len, size_t* update_len);

#endif // PROTOCOL_H
