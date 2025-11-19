# y-websocket Protocol Fix: VarUint Decoding

## The Problem

**Symptom:** Server received updates from clients but couldn't apply them to its document
```
[Server] Received SyncStep2 (74 bytes)
[Document] Failed to apply update (error 4)  ❌
```

**Root Cause:** y-websocket SyncStep2 messages use variable-length integer encoding for the update size, which we weren't decoding.

## y-websocket Protocol Structure

### SyncStep2 Message Format
```
┌──────────────┬─────────────────┬────────────────────┐
│ Message Type │ VarUint Length  │ Actual CRDT Update │
│   (1 byte)   │  (1-5 bytes)    │   (N bytes)        │
│     0x01     │                 │                    │
└──────────────┴─────────────────┴────────────────────┘
```

### VarUint Encoding
Variable-length unsigned integer encoding (LEB128-style):
- Each byte uses 7 bits for data, 1 bit (MSB) as continuation flag
- MSB=1: more bytes follow
- MSB=0: last byte

**Examples:**
```
Value 42:     [0x2A]           = 0010 1010
Value 200:    [0xC8, 0x01]     = 1100 1000, 0000 0001
Value 16384:  [0x80, 0x80, 0x01]
```

## The Fix

### Before (Wrong)
```cpp
// Just skip the message type byte
const unsigned char *payload = data + 1;
ytransaction_apply(txn, payload, len - 1);  // ❌ Includes varUint!
```

### After (Correct)
```cpp
// Decode the varUint length
uint32_t encoded_len = 0;
size_t varuint_bytes = decode_varuint(data + 1, len - 1, &encoded_len);

// Skip BOTH message type AND varUint bytes
size_t payload_offset = 1 + varuint_bytes;
const unsigned char *payload = data + payload_offset;

ytransaction_apply(txn, payload, len - payload_offset);  // ✅ Pure CRDT update
```

### VarUint Decoder Implementation
```cpp
static size_t decode_varuint(const unsigned char *data, size_t len, uint32_t *value) {
    uint32_t num = 0;
    size_t offset = 0;
    uint8_t shift = 0;
    
    while (offset < len) {
        uint8_t byte = data[offset++];
        num |= (byte & 0x7F) << shift;  // Use lower 7 bits
        shift += 7;
        
        if ((byte & 0x80) == 0) {
            // MSB not set = last byte
            *value = num;
            return offset;  // Return bytes consumed
        }
    }
    
    return 0; // Incomplete
}
```

## Example Message Breakdown

**Received:** 74 bytes total
```
[01] [48] [... 72 bytes of CRDT data ...]
 ^    ^
 |    └─ VarUint: 0x48 (MSB=0, value=72)
 └─ Message Type: SyncStep2
```

**Decoding:**
1. Parse message type: `0x01` = SyncStep2
2. Decode varUint: `0x48` = 72 decimal, 1 byte consumed
3. Payload offset: 1 (type) + 1 (varUint) = 2
4. Actual CRDT update: bytes [2..74] = 72 bytes
5. Apply to document: ✅ Success!

## Expected Server Logs (After Fix)

```
[Server] Received SyncStep2 (74 bytes)
[Protocol] Decoded SyncStep2: varUint=72 bytes, offset=1, payload=72
[Document] Applied update (72 bytes)
[Persistence] Saved document (75 bytes)
[Server] Broadcast 74 bytes to N peer(s)
```

## Other Message Types

### SyncStep1 (State Vector Request)
```
[00] [... state vector data ...]
```
- No varUint encoding
- Just skip first byte

### Awareness (Cursors)
```
[02] [... awareness data ...]
```
- Ignored by server (cursor support removed)

## Impact

✅ Server now correctly applies client updates  
✅ Document state properly reconciled on server  
✅ Persistence works (saves real content, not empty doc)  
✅ Clients sync through server as central authority  

## Testing

To verify the fix:

```bash
# 1. Start server
cd server && make run

# 2. Connect with web client
cd ../web-client && npm run dev

# 3. Type in browser
# Expected server logs:
# [Document] Applied update (N bytes)     ← Success!
# [Persistence] Saved document (M bytes)  ← Persisted!

# 4. Ctrl+C to stop server
# 5. Restart server
# Expected: Document loads from disk with your text

# 6. Reconnect client
# Expected: Previous text appears immediately
```

## References

- y-websocket protocol: https://github.com/yjs/y-websocket
- VarUint encoding: LEB128 (Little Endian Base 128)
- Yjs documentation: https://docs.yjs.dev/

