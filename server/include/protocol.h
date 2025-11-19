#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

// y-websocket protocol message types
enum YWebsocketMessageType {
    Y_SYNC_STEP1 = 0,  // Sync request (state vector)
    Y_SYNC_STEP2 = 1,  // Sync response (update)
    Y_AWARENESS = 2    // Awareness update (cursors, presence)
};

// Parse y-websocket message
uint8_t protocol_parse_message_type(const unsigned char *data, size_t len);

// Get payload (skip message type byte)
const unsigned char* protocol_get_payload(const unsigned char *data, size_t len, size_t *payload_len);

// Check if message is a CRDT update (not awareness)
bool protocol_is_crdt_update(uint8_t message_type);

#endif // PROTOCOL_H

