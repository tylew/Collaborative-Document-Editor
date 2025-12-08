# CRDT Server

C++ WebSocket server for collaborative document editing using CRDTs (Conflict-free Replicated Data Types).

## Features

- ✅ **Raw Yjs Protocol**: Direct binary CRDT updates (no complex protocol wrapping)
- ✅ **Document Persistence**: Memory + disk-based (survives restarts)
- ✅ **Modular Architecture**: Separated concerns (peer, document, persistence, server)
- ✅ **Thread-Safe**: OpenMP locks for concurrent operations
- ✅ **Text-Only Sync**: No cursor/awareness overhead

## Architecture

```
server/
├── include/           # Headers
│   ├── peer.h         # Client connection management
│   ├── document.h     # CRDT operations (libyrs)
│   ├── protocol.h     # Message parsing helpers
│   ├── persistence.h  # Disk save/load
│   └── server.h       # WebSocket lifecycle
├── src/              # Implementation
│   ├── main.cpp
│   ├── peer.cpp
│   ├── document.cpp
│   ├── protocol.cpp
│   ├── persistence.cpp
│   └── server.cpp
├── Makefile
└── crdt_document.bin  # Auto-generated persistence
```

## Protocol

Simple custom protocol with raw Yjs updates:

**Client → Server:**
```
[Raw Yjs Binary Update]
```

**Server → Clients:**
```
[Raw Yjs Binary Update]
```

**On Connection:**
- Server sends current document state to new client
- Client sends updates, server applies and broadcasts
- All updates saved to disk

**Shared Type:** `document` (YText)

## Building

### Docker (Recommended)
```bash
docker run --rm -v "$(pwd)":/app crdt-host-dev \
    bash -c "cd /app && make"
```

### Local
Requirements: g++, OpenMP, libwebsockets, libyrs

```bash
make
```

## Running

```bash
# Default port 9000
make run

# Custom port
./crdt_server 8080
```

### With Docker
```bash
docker run --rm -v "$(pwd)":/app -p 9000:9000 crdt-host-dev \
    bash -c "cd /app && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

## Persistence

- **Memory**: Document persists while server runs (survives client disconnects)
- **Disk**: `crdt_document.bin` auto-saved on each update
- **Recovery**: Loads from disk on startup

To reset document:
```bash
rm crdt_document.bin
```

## Testing

1. Start server
2. Connect multiple clients
3. Type in different clients → instant sync
4. Disconnect all clients → server keeps document
5. Restart server → document loads from disk
6. Reconnect → previous content appears

## Key Functions

**document.cpp:**
- `document_apply_update()` - Apply Yjs update to server doc
- `document_get_state()` - Get full document state for new clients

**persistence.cpp:**
- `persistence_save()` - Save to disk
- `persistence_load()` - Load from disk

**peer.cpp:**
- `peers_add()` - Register new client
- `peer_queue_update()` - Queue update for send

**server.cpp:**
- `callback_crdt()` - WebSocket event handler
- `server_broadcast()` - Send to all peers

## Performance

- Thread-safe peer list with OpenMP locks
- Efficient binary protocol (no JSON/text overhead)
- Single shared document (low memory footprint)

## Limitations

- No cursor/awareness sync
- Single document only (shared type: "document")
- No authentication/authorization
- No compression

## Configuration

Edit `main.cpp` to change:
- Port (command line arg)
- Shared type name (default: "document")

## Troubleshooting

**Build errors:**
```bash
# Check dependencies
docker run --rm crdt-host-dev which g++ libyrs
```

**Runtime errors:**
```bash
# Check library path
LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000
```

**Connection refused:**
- Check firewall (port 9000)
- Verify server is listening (`netstat -tlnp | grep 9000`)

## Development

See parent [README.md](../README.md) for full project overview.
