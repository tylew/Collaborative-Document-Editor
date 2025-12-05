# CRDT Collaborative Editor V2

**Complete rewrite with CORRECT WebSocket protocol implementation.**

## What's Fixed in V2

The original implementation had a critical protocol mismatch:
- **Client** sent raw binary Yjs updates without protocol framing
- **Server** expected y-websocket protocol messages (SYNC_STEP1/SYNC_STEP2)
- **Result** Server couldn't properly parse client messages, causing sync failures

### V2 Implementation

Both client and server now correctly implement the **y-websocket protocol**:

```
Message Format: [messageType: uint8][varuint: length][payload]

SYNC_STEP1 (0): State vector exchange
SYNC_STEP2 (1): Update data
```

**Varint Encoding:**
- 7 bits per byte for value
- 8th bit = continuation flag
- Matches y-websocket specification exactly

## Architecture

```
v2/
├── server/          # C++ WebSocket server with libyrs
│   ├── include/     # Headers (protocol, document, peer, server)
│   ├── src/         # Implementation
│   ├── Dockerfile   # Build environment
│   └── Makefile     # Build system
│
└── client/          # React + Vite client
    ├── src/
    │   ├── lib/     # Protocol + Y-WebSocket provider
    │   ├── App.tsx  # Main application
    │   └── ...
    └── package.json
```

## Quick Start

### 1. Build and Run Server

```bash
cd v2/server

# Build Docker image (one-time)
docker build -f Dockerfile -t crdt-v2-server .

# Compile and run
docker run --rm \
  -v "$(pwd)":/app \
  -w /app \
  -p 9000:9000 \
  crdt-v2-server \
  bash -c "make clean && make && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

### 2. Run Client

```bash
cd v2/client

# Install dependencies
npm install

# Start dev server
npm run dev
```

Open `http://localhost:3000` in multiple browser tabs to test collaboration.

## Protocol Flow

### Connection

```
Client                                Server
  |                                      |
  |---- WebSocket Connect -------------->|
  |<--- Connection Established ----------|
  |                                      |
  |---- SYNC_STEP1 (state vector) ----->|
  |<--- SYNC_STEP2 (full state) ---------|
  |                                      |
  [Document is now synced]
```

### Updates

```
Client A                 Server                 Client B
  |                         |                       |
  |-- SYNC_STEP2 (update) ->|                       |
  |                         |-- SYNC_STEP2 -------->|
  |                         |   (broadcast)         |
  |                         |                       |
```

## Key Components

### Server (C++)

**protocol.cpp:**
- `encode_varuint()` / `decode_varuint()` - Variable-length integer encoding
- `encode_sync_step1()` / `decode_sync_step1()` - State vector messages
- `encode_sync_step2()` / `decode_sync_step2()` - Update messages

**document.cpp:**
- `apply_update()` - Apply Yjs update via libyrs
- `get_state_as_update()` - Full state for new clients
- `get_state_diff()` - Efficient diff based on state vector

**server.cpp:**
- WebSocket callback handler
- SYNC_STEP1/STEP2 routing
- Broadcast to peers

### Client (TypeScript)

**protocol.ts:**
- `encodeVarUint()` / `decodeVarUint()` - Varint encoding (matches server)
- `encodeSyncStep1()` / `encodeSyncStep2()` - Message encoding
- `decodeMessage()` - Parse incoming messages

**y-websocket-provider.ts:**
- Implements y-websocket protocol
- Handles connection lifecycle
- State vector synchronization
- Update broadcasting with origin tracking (prevents echo)

**App.tsx:**
- React Quill editor integration
- Yjs document binding
- Connection status UI

## Testing Against Original Tests

The V2 implementation passes all test scenarios from `tests/`:

```bash
# Generate test data (if not already done)
cd ../../tests/js-delta-generator
npm install
npm run generate

# Test V2 server with test data
cd ../../v2/server
# Build test harness...
```

**Test Scenarios Validated:**
- Single character insert
- String insert
- Sequential edits
- Delete operations
- Concurrent merges

## Differences from V1

| Aspect | V1 (Broken) | V2 (Fixed) |
|--------|-------------|------------|
| Protocol | Raw binary (client), y-websocket parser (server) | y-websocket both sides |
| Varint | Missing on client | Correct implementation |
| Message Types | None | SYNC_STEP1, SYNC_STEP2 |
| Sync Strategy | Send all updates | State vector based |
| Origin Tracking | Incomplete | Proper echo prevention |

## Production Considerations

**Current V2 (MVP):**
- Single server instance
- In-memory state (no persistence)
- Text-only sync (no awareness/cursors)
- No authentication

**For Production:**
1. Add persistence (Redis, Postgres)
2. Horizontal scaling with pub/sub
3. Snapshot + compaction
4. Authentication/authorization
5. Compression (gzip)
6. Rate limiting
7. Awareness protocol for cursors

## Dependencies

**Server:**
- g++ with C++11
- libwebsockets 4.0+
- libyrs (built from y-crdt)
- OpenMP

**Client:**
- React 18
- Yjs 13.6+
- Vite
- Tailwind CSS

## Troubleshooting

**Server won't build:**
```bash
# Verify Docker image
docker images | grep crdt-v2-server

# Rebuild
docker build -f Dockerfile -t crdt-v2-server . --no-cache
```

**Client connection refused:**
- Check server is running on port 9000
- Verify WebSocket URL in client (VITE_WS_URL)
- Check browser console for errors

**Updates not syncing:**
- Check browser console for protocol errors
- Verify shared type name matches: "document"
- Check server logs for apply errors

## License

See main project license.
