# y-websocket Protocol Specification

Complete specification for the V2 protocol implementation.

## Message Format

```
┌─────────────┬──────────────┬─────────────────┐
│ Message     │   Varint     │    Payload      │
│ Type (1B)   │  (1-5 bytes) │   (N bytes)     │
└─────────────┴──────────────┴─────────────────┘
```

### Message Types

```
0x00 = SYNC_STEP1  (State Vector Exchange)
0x01 = SYNC_STEP2  (Update Data)
0x02 = AWARENESS   (Cursor/Presence) - Not implemented in V2
```

## Varint Encoding

### Format

```
7-bit value encoding with 8th bit as continuation flag:

Bit Layout:
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │
├───┼───┴───┴───┴───┴───┴───┴───┤
│ C │       Value (7 bits)      │
└───┴───────────────────────────┘

C = Continuation bit (1 = more bytes, 0 = last byte)
```

### Examples

**Example 1: Value = 0**
```
Binary:  00000000
Hex:     0x00
Bytes:   [0x00]
```

**Example 2: Value = 127**
```
Binary:  01111111
Hex:     0x7F
Bytes:   [0x7F]
```

**Example 3: Value = 128**
```
Step 1: 128 = 0b10000000
Step 2: Split into 7-bit chunks: [0000000][0000001]
Step 3: Reverse order: [0000001][0000000]
Step 4: Add continuation bits: [1 0000000][0 0000001]
Result: [0x80, 0x01]
```

**Example 4: Value = 300**
```
Step 1: 300 = 0b100101100
Step 2: Split into 7-bit chunks: [0000010][0101100]
Step 3: Reverse order: [0101100][0000010]
Step 4: Add continuation bits: [1 0101100][0 0000010]
Result: [0xAC, 0x02]

Verification:
  Byte 1: 0xAC = 10101100 → value = 0101100 (44), continue = 1
  Byte 2: 0x02 = 00000010 → value = 0000010 (2), continue = 0
  Result: 44 + (2 << 7) = 44 + 256 = 300 ✓
```

### Encoding Algorithm

```
Input: unsigned integer N
Output: byte array

while N >= 128:
  output (N & 0x7F) | 0x80  // 7 bits + continuation
  N = N >> 7                // shift right 7 bits

output N & 0x7F             // last byte, no continuation
```

### Decoding Algorithm

```
Input: byte array
Output: unsigned integer N

N = 0
shift = 0

for each byte B:
  N |= (B & 0x7F) << shift
  if (B & 0x80) == 0:       // no continuation
    return N
  shift += 7

error: incomplete varint
```

## SYNC_STEP1 (State Vector)

### Purpose
Exchange what each side has, to calculate minimal diff.

### Format
```
┌────┬─────────┬─────────────────┐
│ 00 │ Varint  │ State Vector    │
│    │ (len)   │ (Yjs binary)    │
└────┴─────────┴─────────────────┘
```

### Example

**Empty document (no state):**
```
Hex:  00 00
      │  └─ Varint: 0 (no state vector)
      └─ Message type: SYNC_STEP1

Total: 2 bytes
```

**With state vector (42 bytes):**
```
Hex:  00 2A [... 42 bytes of state vector ...]
      │  │
      │  └─ Varint: 42 (0x2A)
      └─ Message type: SYNC_STEP1

Total: 44 bytes (1 + 1 + 42)
```

### When Sent

**Client to Server:**
- On connect (request initial sync)
- After reconnect (request catch-up)

**Server to Client:**
- Rarely (V2 server sends full state immediately)

## SYNC_STEP2 (Update)

### Purpose
Send actual document changes (Yjs updates).

### Format
```
┌────┬─────────┬─────────────────┐
│ 01 │ Varint  │ Yjs Update      │
│    │ (len)   │ (binary)        │
└────┴─────────┴─────────────────┘
```

### Example

**Small update (67 bytes):**
```
Hex:  01 43 [... 67 bytes of Yjs update ...]
      │  │
      │  └─ Varint: 67 (0x43)
      └─ Message type: SYNC_STEP2

Total: 69 bytes (1 + 1 + 67)
```

**Large update (500 bytes):**
```
Hex:  01 F4 03 [... 500 bytes of Yjs update ...]
      │  └──┴─ Varint: 500 (0x01F4 = [0xF4, 0x03])
      └─ Message type: SYNC_STEP2

Total: 503 bytes (1 + 2 + 500)
```

### When Sent

**Client to Server:**
- On local edit (user types)
- After receiving server's state (if client has newer data)

**Server to Client:**
- On connect (initial state)
- When broadcasting other clients' edits

## Complete Sync Flow

### Initial Connection

```
Time  │ Client                           │ Server
──────┼──────────────────────────────────┼────────────────────────────────
  0   │ WebSocket Connect                │
  1   │ ← Connection Established ────────┤
      │                                  │
  2   │ Prepare state vector:            │
      │   const sv = Y.encodeStateVector(doc)
      │   (empty doc → sv = [])          │
      │                                  │
  3   │ Encode SYNC_STEP1:               │
      │   [0x00][0x00]                   │
      │                                  │
  4   │ ─────────────────────────────────→ Receive SYNC_STEP1
      │                                  │   Parse: type=0, sv=[]
      │                                  │
      │                                  │ Get full state:
      │                                  │   state = doc.get_state_as_update()
      │                                  │   (42 bytes)
      │                                  │
      │                                  │ Encode SYNC_STEP2:
      │                                  │   [0x01][0x2A][... 42 bytes ...]
      │                                  │
  5   │ Receive SYNC_STEP2 ←─────────────┤
      │   Parse: type=1, update=42 bytes │
      │                                  │
      │ Apply update:                    │
      │   Y.applyUpdate(doc, update)     │
      │   Mark synced                    │
      │                                  │
      │ ✓ SYNCED                         │ ✓ CLIENT READY
```

### Collaborative Edit

```
Time  │ Client A              │ Server                │ Client B
──────┼───────────────────────┼───────────────────────┼────────────────────
  0   │ User types "Hello"    │                       │
      │                       │                       │
  1   │ Yjs generates update  │                       │
      │   (67 bytes)          │                       │
      │                       │                       │
  2   │ Encode SYNC_STEP2:    │                       │
      │   [0x01][0x43][...]   │                       │
      │                       │                       │
  3   │ ───────────────────────→ Receive SYNC_STEP2   │
      │                       │   Parse: update=67B   │
      │                       │                       │
      │                       │ Apply to document:    │
      │                       │   doc.apply_update()  │
      │                       │   Content: "Hello"    │
      │                       │                       │
      │                       │ Broadcast to others:  │
      │                       │   (forward same msg)  │
      │                       │                       │
  4   │                       │ ───────────────────────→ Receive SYNC_STEP2
      │                       │                       │   Parse: update=67B
      │                       │                       │
      │                       │                       │ Apply to document:
      │                       │                       │   Y.applyUpdate()
      │                       │                       │   Display: "Hello"
      │                       │                       │
      │ ✓ Sent                │ ✓ Broadcasted         │ ✓ Received
```

## Wire Format Examples

### Example 1: Initial Sync

**Client → Server (SYNC_STEP1 with empty state):**
```
Hex Dump:
00 00

Breakdown:
[00]     Message type: SYNC_STEP1
[00]     Varint: 0 (state vector length)
```

**Server → Client (SYNC_STEP2 with document):**
```
Hex Dump (first 20 bytes):
01 2A 00 01 01 85 B3 E5 D0 F7 0E 00 04 01 64 6F 63 75 6D 65

Breakdown:
[01]           Message type: SYNC_STEP2
[2A]           Varint: 42 (update length)
[00 01 01...]  Yjs update (42 bytes total)
```

### Example 2: Text Insert

**Client → Server (Insert "Hello"):**
```
Hex Dump (first 20 bytes):
01 43 00 01 01 9A B7 D3 A4 0B 00 05 01 05 48 65 6C 6C 6F

Breakdown:
[01]           Message type: SYNC_STEP2
[43]           Varint: 67 (update length)
[00 01 01...]  Yjs update containing "Hello" (67 bytes total)
```

## Error Handling

### Invalid Message Type

```
Received: [0xFF][...]

Action:
- Log error: "Unknown message type: 255"
- Close connection
```

### Incomplete Varint

```
Received: [0x01][0x80][0x80][0x80][0x80]
                └────────────────────┘
                All have continuation bit, no terminator

Action:
- Return error: "Incomplete varint"
- Drop message
```

### Payload Size Mismatch

```
Received: [0x01][0x64][... 50 bytes ...]
                  │     └─ Only 50 bytes
                  └─ Claims 100 bytes (0x64)

Action:
- Return error: "Incomplete message"
- Buffer and wait for more data
```

## Performance Analysis

### Message Overhead

| Payload Size | Type (1B) | Varint | Total Overhead | Overhead % |
|--------------|-----------|--------|----------------|------------|
| 0 bytes      | 1         | 1      | 2              | ∞          |
| 10 bytes     | 1         | 1      | 2              | 20%        |
| 100 bytes    | 1         | 1      | 2              | 2%         |
| 127 bytes    | 1         | 1      | 2              | 1.6%       |
| 128 bytes    | 1         | 2      | 3              | 2.3%       |
| 1 KB         | 1         | 2      | 3              | 0.3%       |
| 10 KB        | 1         | 2      | 3              | 0.03%      |

**Conclusion:** Overhead is negligible for typical document updates (>100 bytes).

### Varint Space Efficiency

| Value Range | Bytes | Efficiency vs Fixed 4-byte |
|-------------|-------|----------------------------|
| 0-127       | 1     | 75% savings                |
| 128-16,383  | 2     | 50% savings                |
| 16,384-2M   | 3     | 25% savings                |
| 2M-268M     | 4     | 0% (same)                  |
| 268M+       | 5     | -25% (worse)               |

**Conclusion:** Highly efficient for typical document sizes (<2MB updates).

## Implementation Checklist

### Encoder

- [x] Varint encoding
- [x] SYNC_STEP1 encoding
- [x] SYNC_STEP2 encoding
- [ ] AWARENESS encoding (future)
- [x] Overflow protection
- [x] Memory management

### Decoder

- [x] Varint decoding
- [x] Message type parsing
- [x] SYNC_STEP1 decoding
- [x] SYNC_STEP2 decoding
- [ ] AWARENESS decoding (future)
- [x] Error handling
- [x] Bounds checking

### Testing

- [ ] Unit tests (varint round-trip)
- [ ] Message encode/decode tests
- [ ] Fuzzing (random inputs)
- [ ] Interop tests (JS ↔ C++)
- [x] Manual end-to-end testing

## References

- **y-websocket protocol:** https://github.com/yjs/y-websocket
- **Varint encoding:** https://developers.google.com/protocol-buffers/docs/encoding#varints
- **Yjs library:** https://github.com/yjs/yjs
- **libyrs (Rust):** https://github.com/y-crdt/y-crdt

## Appendix: Binary Format Details

### Yjs Update Structure (Not Parsed by Protocol)

The payload in SYNC_STEP2 is a Yjs binary update. The protocol doesn't parse this - it's passed directly to Yjs/libyrs.

**Typical structure:**
```
[Yjs Header]
[Version]
[Client ID]
[Clock]
[Operations]
[Delete Set]
```

**We treat as opaque binary blob:**
```
Protocol Layer: [0x01][varint][OPAQUE_BLOB]
                                └─ Passed to Yjs
```

### State Vector Structure (Not Parsed by Protocol)

The payload in SYNC_STEP1 is a Yjs state vector. Same as update - opaque to protocol.

**Typical structure:**
```
[Version]
[Client Count]
[Client ID 1][Clock 1]
[Client ID 2][Clock 2]
...
```

**We treat as opaque:**
```
Protocol Layer: [0x00][varint][OPAQUE_BLOB]
                                └─ Passed to Yjs
```

This separation of concerns keeps protocol layer simple and Yjs-agnostic.
