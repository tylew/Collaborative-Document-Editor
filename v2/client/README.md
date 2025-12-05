# V2 CRDT Client

React + TypeScript client with correct y-websocket protocol implementation.

## What's Fixed

**V1 Problem:**
- Sent raw binary Yjs updates without protocol framing
- No SYNC_STEP1/SYNC_STEP2 message types
- Server couldn't parse messages

**V2 Solution:**
- Implements complete y-websocket protocol
- Proper varint encoding
- SYNC_STEP1/STEP2 handshake
- Origin tracking to prevent echo

## Architecture

```
client/
├── src/
│   ├── lib/
│   │   ├── protocol.ts             # Varint + message encode/decode
│   │   └── y-websocket-provider.ts # WebSocket provider
│   ├── App.tsx                     # Main application
│   ├── main.tsx                    # React entry point
│   ├── index.css                   # Tailwind + Quill styles
│   └── vite-env.d.ts              # Type definitions
├── index.html
├── package.json
├── vite.config.ts
├── tailwind.config.js
└── tsconfig.json
```

## Protocol Implementation

### protocol.ts

**Varint Encoding:**
```typescript
encodeVarUint(300) // → [0xAC, 0x02]
decodeVarUint([0xAC, 0x02]) // → [300, 2]
```

**Message Encoding:**
```typescript
// SYNC_STEP1: [0][varuint: sv_len][state_vector]
encodeSyncStep1(stateVector)

// SYNC_STEP2: [1][varuint: update_len][update]
encodeSyncStep2(update)
```

**Message Decoding:**
```typescript
decodeMessage(data) // → { type: MessageType, payload: Uint8Array }
```

### y-websocket-provider.ts

**Connection Flow:**
```typescript
1. WebSocket connects
2. Send SYNC_STEP1 with our state vector
3. Receive SYNC_STEP2 with server's diff
4. Apply update, mark as synced
5. Listen for local changes, send as SYNC_STEP2
```

**Origin Tracking:**
```typescript
// When applying remote update
Y.applyUpdate(doc, update, this); // origin=this

// When sending local update
doc.on('update', (update, origin) => {
  if (origin !== this) { // Don't send our own updates back
    sendUpdate(update);
  }
});
```

## Setup

```bash
npm install
npm run dev
```

Open `http://localhost:3000`

## Configuration

**Environment Variables:**

Create `.env`:
```bash
VITE_WS_URL=ws://localhost:9000
```

**Vite Config:**
```typescript
// vite.config.ts
server: {
  port: 3000,
  host: true,
}
```

## Key Components

### YWebSocketProvider

```typescript
const provider = new YWebSocketProvider(wsUrl, ydoc);

provider.on('status', (event) => {
  // 'connecting' | 'connected' | 'disconnected' | 'error'
});

provider.on('synced', () => {
  // Document is now synced with server
});

provider.destroy(); // Cleanup
```

**Features:**
- Automatic reconnection (exponential backoff)
- Max reconnect attempts: 10
- Initial delay: 1 second
- Max delay: 5 seconds

### App.tsx

**Yjs Integration:**
```typescript
const ydoc = new Y.Doc();
const ytext = ydoc.getText('document'); // MUST match server

// Yjs -> Quill (remote changes)
ytext.observe(() => {
  const newText = ytext.toString();
  setEditorValue(newText);
});

// Quill -> Yjs (local changes)
const handleEditorChange = (content) => {
  ydoc.transact(() => {
    ytext.delete(0, currentText.length);
    ytext.insert(0, content);
  });
};
```

## Debugging

**Browser Console:**
```
[Provider] Connecting to ws://localhost:9000
[Provider] Connected
[Provider] Sending SYNC_STEP1 (state vector: 42 bytes)
[Provider] Received SYNC_STEP2 (update: 1234 bytes)
[Provider] Document synced
[Provider] Sending update: 56 bytes
```

**Common Issues:**

**Connection refused:**
- Check server is running
- Verify VITE_WS_URL
- Check browser network tab

**Updates not syncing:**
- Check shared type name: "document"
- Verify origin tracking (should see origin=this in console)
- Check for JavaScript errors

**Quill not updating:**
- Check isRemoteChange flag
- Verify ytext.observe() is called
- Check React StrictMode (may cause double renders)

## Testing

**Multi-Client:**
1. Open multiple browser tabs
2. Type in one tab
3. Should instantly appear in others

**Reconnection:**
1. Stop server
2. Client should show "disconnected"
3. Restart server
4. Client should reconnect and sync

**State Recovery:**
1. Type in client A
2. Open client B
3. Client B should receive full state

## Protocol Messages

**Message Structure:**
```
Byte 0: Message Type
  0 = SYNC_STEP1
  1 = SYNC_STEP2
  2 = AWARENESS (not implemented)

Bytes 1+: Varint (payload length)
  7 bits per byte for value
  8th bit = continuation flag

Bytes N+: Payload
  SYNC_STEP1: State vector (binary)
  SYNC_STEP2: Yjs update (binary)
```

**Example SYNC_STEP2:**
```
[01] [8E 03] [00 01 02 ...]
 │     │      │
 │     │      └─ Update data (398 bytes)
 │     └─ Varint: 398 (0x18E)
 └─ Message type: SYNC_STEP2
```

## Performance

**Optimization:**
- Efficient varint encoding (no string conversion)
- Zero-copy buffer operations where possible
- Debounced Quill updates (built-in)
- Yjs CRDT handles conflicts automatically

**Bundle Size:**
- yjs: ~68 KB
- react-quill: ~150 KB
- Total (gzipped): ~80 KB

## Production Build

```bash
npm run build
```

Output in `dist/`:
- HTML, CSS, JS minified
- Assets fingerprinted
- Source maps included

**Serve:**
```bash
npm run preview
# or
npx serve dist
```

## Limitations

**Current V2:**
- Text-only sync (no rich formatting)
- No awareness (cursors/presence)
- No offline support
- No conflict UI (CRDT handles automatically)

**Future:**
- Implement awareness protocol
- Sync Quill deltas (formatting)
- IndexedDB for offline
- Show conflict indicators

## Dependencies

**Runtime:**
- react 18.2+ - UI framework
- yjs 13.6+ - CRDT library
- react-quill 2.0+ - Editor component
- quill 1.3+ - Rich text editor

**Dev:**
- vite 5+ - Build tool
- typescript 5+ - Type safety
- tailwindcss 3+ - Styling

## Browser Support

- Chrome/Edge 90+
- Firefox 88+
- Safari 14+

Requires:
- WebSocket API
- ES2020
- ArrayBuffer / TypedArrays

## License

See main project license.
