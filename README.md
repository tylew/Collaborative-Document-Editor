# CRDT Collaborative Document Editor

Real-time collaborative text editor using CRDTs (Conflict-free Replicated Data Types) with a custom C++ WebSocket server and React web client.

## Architecture

```
┌─────────────────┐      WebSocket (Raw Yjs Updates)     ┌──────────────────┐
│  React Client   │ ◄──────────────────────────────────► │   C++ Server     │
│  (TypeScript)   │                                      │   (libyrs)       │
│  + Yjs CRDT     │ ◄──────────────────────────────────► │   + libwebsockets│
└─────────────────┘                                      └──────────────────┘
    Quill Editor                                         Document Persistence
```

## Components

### [Server](./server/) 
C++ WebSocket server that maintains master CRDT document state
- **Technology**: C++, libyrs (C FFI bindings to the Rust Yrs CRDT engine, compatible with the Yjs project), libwebsockets, OpenMP
- **Features**: Document persistence, multi-client sync, thread-safe operations
- **Port**: 9000 (default)

### [Web Client](./web-client/)
React-based collaborative editor interface
- **Technology**: React 18, TypeScript, Tailwind CSS, Yjs, React-Quill
- **Features**: Real-time sync, rich text editing, connection status
- **Port**: 3000 (default)

## Quick Start

### Server
```bash
cd server
docker run --rm -v "$(pwd)":/app -p 9000:9000 crdt-host-dev \
    bash -c "cd /app && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

### Client
```bash
cd web-client
npm install
npm run dev
```

Open `http://localhost:3000` in multiple browser tabs to test collaboration.

## Key Features

- **CRDT-based**: Conflict-free synchronization using Yjs
- **Real-time**: Instant updates across all connected clients
- **Persistent**: Server maintains document state (memory + disk)
- **Modular**: Clean separation between server and client
- **Type-safe**: Full TypeScript on client, C++ on server

## Protocol

Custom simplified protocol sending raw Yjs updates over WebSocket:
- Client sends: Raw Yjs binary updates
- Server: Applies to master document, broadcasts to peers
- No awareness/cursor sync (text-only)

## Documentation

- **Server Details**: [server/README.md](./server/README.md)
- **Client Details**: [web-client/README.md](./web-client/README.md)

## Development

See individual component READMEs for:
- Build instructions
- Configuration options
- Architecture details
- API documentation

## Necessary production enhancements
1.	Stateless replication via external CRDT log

Remove authoritative in-memory state from the C++ process. Persist all Yjs updates as an append-only log (Redis Streams or Postgres bytea). On startup, reconstruct the full Yjs document by replaying log entries. All Cloud Run instances consume the same ordered log, guaranteeing deterministic convergence.

2.	Snapshot + compaction pipeline

Add periodic CRDT snapshotting (encode full Yjs state). Store snapshot in Redis/Postgres/GCS, then truncate older log segments. Prevents unbounded replay time and reduces cold-start latency under serverless.

3.	Horizontal fan-out mechanism

Each server instance only has local WebSocket clients. To deliver global updates, implement a pub/sub path: server writes update to log → broadcasts locally → other instances pull new log entries and push to their clients. Prevents divergence across autoscaled instances.

4.	Dedicated persistence layer

Choose one durable backend:
- Redis Streams (at-least-once, ordered)
- Postgres logical log table
- Firestore (last-ditch, not suited for binary deltas)

All production deployments require strict ordering guarantees for CRDT operations.

## License

See project license file.

