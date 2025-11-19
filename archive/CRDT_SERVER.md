# CRDT Document Server

A WebSocket-based collaborative document server using Y-CRDT (Yrs) for conflict-free synchronization.

## Overview

`crdt_server.cpp` implements a multi-client collaborative document server where:
- A single master CRDT document is maintained on the server
- Multiple clients can connect via WebSocket and make concurrent edits
- All changes are automatically reconciled using CRDT conflict resolution
- Updates are broadcast to all connected clients in real-time

## Architecture

### Key Components

1. **Master Document (`g_master_doc`)**: Single YDoc instance that holds the authoritative state
2. **Shared Text (`g_text`)**: YText object named "document" for collaborative text editing
3. **Peer Management**: Linked list of connected clients with thread-safe access
4. **Update Broadcasting**: Parallel broadcast using OpenMP for efficient fan-out

### Comparison with host.cpp

| Feature | host.cpp | crdt_server.cpp |
|---------|----------|-----------------|
| **Document Type** | Generic YDoc | YDoc with YText for text editing |
| **Sync Strategy** | Manual broadcast | CRDT-aware with state vectors |
| **Initial Sync** | Full state diff | Full state diff (optimized) |
| **Update Handling** | Direct broadcast | Apply then broadcast |
| **Client Tracking** | Basic peer list | Sync status tracking |
| **Loop Prevention** | None | `g_applying_remote_update` flag |

### How It Works

```
Client A                 Server                   Client B
   |                       |                          |
   |--- connect ---------->|                          |
   |<-- full state --------|                          |
   |                       |<-------- connect --------|
   |                       |-------- full state ----->|
   |                       |                          |
   |--- text insert ------>|                          |
   |                       |--- apply update          |
   |                       |--- broadcast update ---->|
   |<-- confirmation ------|                          |
   |                       |                          |
```

## Building

### Using Make

```bash
make crdt-server
```

### Manual Build

```bash
g++ -O2 -fopenmp crdt_server.cpp -o crdt_server \
    -lwebsockets -lyrs -pthread \
    -I/usr/local/include -L/usr/local/lib
```

### Docker

```bash
docker run --rm -v "$(pwd)":/app crdt-host-dev bash -c \
    "cd /app && make crdt-server"
```

## Running

### Start the Server

```bash
./crdt_server [port]
# Default port: 9000
```

Example:
```bash
./crdt_server 8080
```

### With Docker

```bash
docker run --rm -v "$(pwd)":/app -p 9000:9000 crdt-host-dev \
    bash -c "cd /app && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

## Protocol

### WebSocket Protocol: `crdt-proto`

The server uses binary WebSocket messages containing Yrs update payloads.

### Connection Flow

1. **Client Connects**
   - Server sends full document state as binary update
   - Client applies update to sync with server

2. **Client Sends Update**
   - Client encodes local changes as Yrs update
   - Sends binary update to server
   - Server applies update to master document
   - Server broadcasts update to all other clients

3. **Client Receives Update**
   - Client applies binary update to local document
   - CRDT ensures automatic conflict resolution

## API Usage Examples

### JavaScript Client (using yjs)

```javascript
import * as Y from 'yjs'
import { WebsocketProvider } from 'y-websocket'

const doc = new Y.Doc()
const provider = new WebsocketProvider('ws://localhost:9000', 'crdt-proto', doc)

const text = doc.getText('document')
text.insert(0, 'Hello, collaborative world!')

// Changes are automatically synchronized
```

### C++ Client (using yffi)

```cpp
YDoc* doc = ydoc_new();
Branch* text = ytext(doc, "document");

// Connect to WebSocket and set up observer
YSubscription* sub = ydoc_observe_updates_v1(doc, NULL, send_to_server_callback);

// Make changes
YTransaction* txn = ydoc_write_transaction(doc, 0, NULL);
ytext_insert(text, txn, 0, "Hello", NULL);
ytransaction_commit(txn);
```

## State Vector Optimization

The server supports efficient synchronization using state vectors:

1. **Initial Sync**: Client sends state vector → Server sends only missing updates
2. **Reconnection**: Client maintains state → Only delta is transmitted
3. **Bandwidth**: Minimal data transfer for large documents

## Key Features

### Thread Safety
- OpenMP locks protect peer list
- Per-peer locks for pending data
- Atomic flags for update loop prevention

### Performance
- Parallel broadcast using OpenMP
- Lock-free read operations where possible
- Efficient binary protocol

### Reliability
- Automatic CRDT conflict resolution
- No message ordering requirements
- Handles concurrent edits gracefully

## Testing

### Test with sync_example.cpp

The `sync_example.cpp` demonstrates the CRDT synchronization patterns used by the server:

```bash
make sync-example
./sync_example
```

### Test with Multiple Clients

See `client.cpp` for a full-featured C++ client implementation that connects to the server.

## Debugging

Enable debug output:
```cpp
// In crdt_server.cpp, uncomment:
print_document_content();  // Shows periodic document state
```

Monitor updates:
```bash
# Watch server logs
./crdt_server 9000 | tee server.log
```

## Limitations

1. **In-Memory Only**: Document state is not persisted
2. **Single Document**: One shared text document per server instance
3. **No Authentication**: All clients can read/write

## Future Enhancements

- [ ] Persistent document storage
- [ ] Multiple document support
- [ ] User authentication and permissions
- [ ] Rich text formatting (attributes)
- [ ] Document history/undo
- [ ] State vector-based initial sync handshake

## See Also

- `sync_example.cpp` - CRDT synchronization examples
- `host.cpp` - Original simpler implementation
- `client.cpp` - Full C++ client
- [Yrs Documentation](https://docs.rs/yrs/)
- [Yjs Documentation](https://docs.yjs.dev/)

