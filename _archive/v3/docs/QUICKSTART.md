# V2 Quick Start Guide

Get the CRDT collaborative editor running in 5 minutes.

## Prerequisites

- Docker (for server)
- Node.js 18+ (for client)

## Step 1: Start Server

```bash
cd v2/server

# Build Docker image (one-time, ~5 minutes)
docker build -f Dockerfile -t crdt-v2-server .

# Run server (compiles + starts)
docker run --rm \
  -v "$(pwd)":/app \
  -w /app \
  -p 9000:9000 \
  crdt-v2-server \
  bash -c "make clean && make && LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

**Expected output:**
```
========================================
CRDT WebSocket Server v2
========================================
Starting server on port 9000...
[Server] Listening on port 9000
[Server] Shared type: 'document'
[Server] Protocol: y-websocket (SYNC_STEP1/SYNC_STEP2)
```

## Step 2: Start Client

**In a new terminal:**

```bash
cd v2/client

# Install dependencies (one-time, ~2 minutes)
npm install

# Start dev server
npm run dev
```

**Expected output:**
```
VITE v5.2.0  ready in 1234 ms

➜  Local:   http://localhost:3000/
➜  Network: use --host to expose
```

## Step 3: Test Collaboration

1. Open `http://localhost:3000` in your browser
2. Open same URL in another tab (or browser)
3. Type in one editor → text appears in the other instantly

**What you should see:**

```
Tab 1: Types "Hello"
       ↓
Server: Receives SYNC_STEP2, applies update, broadcasts
       ↓
Tab 2: Receives SYNC_STEP2, displays "Hello"
```

## Verify It's Working

**Server logs:**
```
[Server] Client connected (total: 1)
[Server] Queued initial state (42 bytes) for new client
[Server] Sent 58 bytes to client
[Server] Received SYNC_STEP2 (67 bytes)
[Server] Applied update (67 bytes)
[Server] Document content: "Hello"
[Server] Broadcast 67 bytes to 0 peer(s)
```

**Browser console (F12):**
```
[Provider] Connecting to ws://localhost:9000
[Provider] Connected
[Provider] Sending SYNC_STEP1 (state vector: 0 bytes)
[Provider] Received SYNC_STEP2 (update: 42 bytes)
[Provider] Document synced
[Provider] Sending update: 67 bytes
```

## Common Issues

### Server won't start

**Error:** `docker: command not found`
```bash
# Install Docker
# https://docs.docker.com/engine/install/
```

**Error:** `Port 9000 already in use`
```bash
# Find process using port
lsof -i :9000

# Kill it or use different port
docker run ... crdt_server 9001  # change port
```

### Client won't start

**Error:** `npm: command not found`
```bash
# Install Node.js
# https://nodejs.org/
```

**Error:** `Cannot find module 'yjs'`
```bash
# Reinstall dependencies
rm -rf node_modules package-lock.json
npm install
```

### Connection fails

**Browser shows:** "Disconnected"

**Check:**
1. Server is running: `docker ps` should show container
2. Port is correct: Server log should say "Listening on port 9000"
3. URL is correct: Browser console should show `ws://localhost:9000`

### Updates not syncing

**Symptoms:** Type in one tab, doesn't appear in other

**Check:**
1. Browser console for errors
2. Server logs for "Applied update"
3. Shared type matches: "document" (both sides)

**Fix:**
```bash
# Restart server
Ctrl+C in server terminal, then re-run docker command

# Hard refresh browser
Ctrl+Shift+R (or Cmd+Shift+R on Mac)
```

## Next Steps

- Read [v2/README.md](./README.md) for architecture details
- Check [server/README.md](./server/README.md) for protocol spec
- Check [client/README.md](./client/README.md) for client API

## Testing Multiple Clients

**Option 1: Multiple tabs**
- Same browser, different tabs
- Shares same session

**Option 2: Different browsers**
- Chrome + Firefox
- Better isolation

**Option 3: Different machines**
- Change `VITE_WS_URL` to server IP
- Make sure port 9000 is accessible

## Stopping

**Server:**
```bash
Ctrl+C in server terminal
# Container auto-removes (--rm flag)
```

**Client:**
```bash
Ctrl+C in client terminal
```

## Development Workflow

**Edit server code:**
```bash
# Edit files in v2/server/src/
# Recompile
docker run --rm -v "$(pwd)":/app -w /app crdt-v2-server make
# Run
docker run --rm -v "$(pwd)":/app -w /app -p 9000:9000 crdt-v2-server \
  bash -c "LD_LIBRARY_PATH=/usr/local/lib ./crdt_server 9000"
```

**Edit client code:**
```bash
# Edit files in v2/client/src/
# Vite auto-reloads (hot module replacement)
```

## Production Build

**Server:**
```bash
# Same as dev (binary is optimized with -O2)
```

**Client:**
```bash
cd v2/client
npm run build
# Output in dist/

# Serve
npm run preview
# or
npx serve dist
```

## Help

**Questions?**
- Check READMEs in each directory
- Look at comments in source code
- Compare with test files in `/tests`

**Found a bug?**
- Check logs (server + browser console)
- Verify protocol messages match
- Compare with working test cases
