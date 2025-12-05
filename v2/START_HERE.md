# V2 Implementation - Start Here

**Status:** Complete and ready to test

## What Was Built

A complete rewrite of the CRDT collaborative editor with **correct WebSocket protocol implementation**.

The original system had a critical flaw: the client sent raw binary data while the server expected y-websocket protocol messages. V2 fixes this by implementing the proper protocol on both sides.

## Quick Start (5 minutes)

### 1. Start Server

```bash
cd server

# Build Docker image (one-time, ~3-5 minutes)
docker build -f Dockerfile -t crdt-v2-server .

# Run server
docker run --rm \
  -v "$(pwd)":/app \
  -w /app \
  -p 9000:9000 \
  crdt-v2-server \
  bash -c "make clean && make && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

### 2. Start Client (new terminal)

```bash
cd client

# Install dependencies (one-time, ~2 minutes)
npm install

# Run dev server
npm run dev
```

### 3. Test

Open `http://localhost:3000` in 2+ browser tabs and type in one - text appears instantly in the others.

## What's Inside

```
v2/
├── START_HERE.md              ← You are here
├── QUICKSTART.md              ← Detailed setup guide
├── README.md                  ← Architecture overview
├── PROTOCOL.md                ← Complete protocol specification
├── COMPARISON.md              ← V1 vs V2 analysis
├── IMPLEMENTATION_SUMMARY.md  ← Implementation details
│
├── server/                    ← C++ WebSocket server
│   ├── README.md              ← Server documentation
│   ├── Dockerfile             ← Build environment
│   ├── Makefile               ← Build system
│   ├── include/               ← Headers
│   │   ├── protocol.h         ← y-websocket protocol
│   │   ├── document.h         ← CRDT document (libyrs)
│   │   ├── peer.h             ← Client management
│   │   └── server.h           ← WebSocket server
│   └── src/                   ← Implementation
│       ├── protocol.cpp       ← Varint + message encode/decode
│       ├── document.cpp       ← Yjs operations via libyrs
│       ├── peer.cpp           ← Thread-safe peer list
│       ├── server.cpp         ← WebSocket callbacks
│       └── main.cpp           ← Entry point
│
└── client/                    ← React + TypeScript client
    ├── README.md              ← Client documentation
    ├── package.json           ← Dependencies
    ├── vite.config.ts         ← Vite configuration
    ├── index.html             ← HTML template
    └── src/
        ├── lib/
        │   ├── protocol.ts              ← Protocol (matches server)
        │   └── y-websocket-provider.ts  ← WebSocket provider
        ├── App.tsx            ← React app with Quill
        ├── main.tsx           ← React entry
        └── index.css          ← Tailwind + Quill styles
```

## Key Files to Read

1. **QUICKSTART.md** - Get it running in 5 minutes
2. **PROTOCOL.md** - Understand the protocol format
3. **COMPARISON.md** - See what was broken and how it's fixed
4. **server/README.md** - Server implementation details
5. **client/README.md** - Client implementation details

## The Fix

### V1 Problem

```
Client:  ws.send(rawBinary)        Server: Expects [type][varint][payload]
         │                                 │
         └─ No protocol framing            └─ Can't parse ✗
```

### V2 Solution

```
Client:  encodeSyncStep2(update)   Server: decode_sync_step2()
         │                                 │
         └─ [0x01][varint][update]         └─ Parses correctly ✓
```

Both sides now speak the same language.

## Protocol Overview

**Message Format:**
```
[messageType: 1 byte][varUint: 1-5 bytes][payload: N bytes]
```

**Message Types:**
- `0x00` = SYNC_STEP1 (state vector)
- `0x01` = SYNC_STEP2 (update)

**Varint:** Variable-length integer encoding (7 bits per byte, 8th bit = continuation)

**Example:**
```
SYNC_STEP2 with 300-byte update:
[0x01][0xAC 0x02][... 300 bytes ...]
  │     └──┴─ Varint: 300
  └─ Message type: SYNC_STEP2
```

## Sync Flow

```
1. Client connects
2. Client sends SYNC_STEP1 (what do I have?)
3. Server responds with SYNC_STEP2 (here's what you need)
4. Client applies update, marks synced
5. On edit: Client sends SYNC_STEP2
6. Server applies, broadcasts to other clients
```

## Implementation Stats

- **Total Files:** 31
- **Lines of Code:** ~2000 (excluding docs)
- **Documentation:** ~5000 lines across 6 comprehensive guides
- **Server:** 800 lines (C++)
- **Client:** 500 lines (TypeScript/React)
- **Protocol:** Identical implementation on both sides

## Technology Stack

**Server:**
- C++11
- libwebsockets (WebSocket server)
- libyrs (Yrs C FFI - CRDT engine)
- OpenMP (thread safety)
- Docker (build environment)

**Client:**
- React 18
- TypeScript 5
- Yjs 13 (CRDT library)
- React-Quill (editor)
- Vite 5 (build tool)
- Tailwind CSS (styling)

## Verification

When running correctly, you should see:

**Server logs:**
```
[Server] Listening on port 9000
[Server] Shared type: 'document'
[Server] Protocol: y-websocket (SYNC_STEP1/SYNC_STEP2)
[Server] Client connected (total: 1)
[Server] Queued initial state (42 bytes) for new client
[Server] Received SYNC_STEP2 (67 bytes)
[Server] Applied update (67 bytes)
[Server] Document content: "Hello"
```

**Browser console:**
```
[Provider] Connected
[Provider] Sending SYNC_STEP1 (state vector: 0 bytes)
[Provider] Received SYNC_STEP2 (update: 42 bytes)
[Provider] Document synced
[Provider] Sending update: 67 bytes
```

## Testing Checklist

- [ ] Server compiles without errors
- [ ] Client installs without errors
- [ ] Server starts and listens on port 9000
- [ ] Client connects (status shows "Connected & Synced")
- [ ] Type in tab 1 → appears in tab 2
- [ ] Type in tab 2 → appears in tab 1
- [ ] Close tab 1 → tab 2 continues working
- [ ] Reopen tab 1 → receives current document state

## Troubleshooting

**Server won't build:**
```bash
# Rebuild Docker image
docker build -f server/Dockerfile -t crdt-v2-server . --no-cache
```

**Client won't connect:**
```bash
# Check server is running
docker ps

# Check logs
# Server should show "Listening on port 9000"
# Browser console should show "[Provider] Connected"
```

**Updates don't sync:**
```bash
# Hard refresh browser (Ctrl+Shift+R)
# Restart server
# Check browser console for errors
```

## Next Steps

1. **Read QUICKSTART.md** for detailed setup
2. **Test the system** with multiple clients
3. **Read PROTOCOL.md** to understand the protocol
4. **Read COMPARISON.md** to see what was fixed
5. **Review source code** with inline comments

## Production Considerations

Current V2 is an MVP suitable for:
- Internal tools
- Demos
- Development

For production deployment, add:
- Persistence (Redis/Postgres)
- Authentication
- Rate limiting
- WSS (secure WebSocket)
- Horizontal scaling (pub/sub)
- Monitoring

See IMPLEMENTATION_SUMMARY.md for details.

## Questions?

All documentation is in this directory:
- Architecture → README.md
- Protocol details → PROTOCOL.md
- Setup guide → QUICKSTART.md
- V1 vs V2 → COMPARISON.md
- Implementation → IMPLEMENTATION_SUMMARY.md
- Server specifics → server/README.md
- Client specifics → client/README.md

## Success Criteria

You'll know V2 is working when:
1. Server compiles and runs
2. Client connects and shows "Connected & Synced"
3. Typing in one browser tab instantly appears in another
4. Multiple clients can edit simultaneously
5. Document state is preserved across client reconnects

**Ready to test?** Run the Quick Start commands above!
