
const WebSocket = require('ws');
const Y = require('yjs');

const SERVER_URL = 'ws://localhost:9000';

// Protocol Constants
const MSG_SYNC_STEP1 = 0;
const MSG_SYNC_STEP2 = 1;
const MSG_AWARENESS = 2;

// --- VarInt Helper Functions ---
function encodeVarUint(num) {
    const res = [];
    while (num >= 0x80) {
        res.push((num & 0x7f) | 0x80);
        num >>>= 7;
    }
    res.push(num & 0x7f);
    return new Uint8Array(res);
}

function decodeVarUint(data, offset) {
    let result = 0;
    let shift = 0;
    let pos = offset;

    while (true) {
        const byte = data[pos++];
        result |= (byte & 0x7f) << shift;
        if ((byte & 0x80) === 0) {
            return [result, pos];
        }
        shift += 7;
    }
}

// --- Benchmark Client ---
class BenchBot {
    constructor(id) {
        this.id = id;
        this.name = `BenchBot-${id}`;
        this.doc = new Y.Doc();
        this.ytext = this.doc.getText('quill');
        this.ws = null;
        this.synced = false;
        this.connected = false;
        this.onLatencyMeasure = null; // Callback when a marker is received
    }

    connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(SERVER_URL);
            this.ws.binaryType = 'arraybuffer';

            this.ws.on('open', () => {
                this.connected = true;
                this.startSync();
                this.sendAwareness();
                resolve();
            });

            this.ws.on('message', (data) => this.handleMessage(new Uint8Array(data)));
            this.ws.on('error', (err) => console.error(`[${this.name}] Error:`, err.message));
        });
    }

    disconnect() {
        if (this.ws) this.ws.terminate();
    }

    startSync() {
        const vector = Y.encodeStateVector(this.doc);
        const len = encodeVarUint(vector.length);
        const msg = new Uint8Array(1 + len.length + vector.length);
        msg[0] = MSG_SYNC_STEP1;
        msg.set(len, 1);
        msg.set(vector, 1 + len.length);
        this.ws.send(msg);
    }

    sendAwareness() {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
        // Minimal awareness to be counted as a peer
        const state = { user: { name: this.name, color: '#000000' } };
        const jsonBytes = Buffer.from(JSON.stringify(state), 'utf8');
        const clientIdBytes = encodeVarUint(this.id); 
        const jsonLenBytes = encodeVarUint(jsonBytes.length);
        const payloadLen = clientIdBytes.length + jsonLenBytes.length + jsonBytes.length;
        const payloadLenBytes = encodeVarUint(payloadLen);
        const msg = new Uint8Array(1 + payloadLenBytes.length + payloadLen);
        let pos = 0;
        msg[pos++] = MSG_AWARENESS;
        msg.set(payloadLenBytes, pos); pos += payloadLenBytes.length;
        msg.set(clientIdBytes, pos); pos += clientIdBytes.length;
        msg.set(jsonLenBytes, pos); pos += jsonLenBytes.length;
        msg.set(jsonBytes, pos);
        this.ws.send(msg);
    }

    sendMarker(markerId) {
        // Sends a specific text insert that acts as a timestamp marker
        const timestamp = Date.now();
        const content = `{{MARKER:${markerId}:${timestamp}}}`;
        
        // We construct the update manually via Yjs
        const update = Y.encodeStateAsUpdate(this.doc); // Just to get a baseline? No.
        
        // We need to transact
        let updateToSend = null;
        this.doc.on('update', (update) => {
            updateToSend = update;
        });
        
        this.doc.transact(() => {
            this.ytext.insert(this.ytext.length, content);
        });
        
        if (updateToSend) {
            const len = encodeVarUint(updateToSend.length);
            const msg = new Uint8Array(1 + len.length + updateToSend.length);
            msg[0] = MSG_SYNC_STEP2;
            msg.set(len, 1);
            msg.set(updateToSend, 1 + len.length);
            this.ws.send(msg);
            return timestamp;
        }
        return null;
    }

    handleMessage(data) {
        try {
            const type = data[0];
            const [len, offset] = decodeVarUint(data, 1);
            const payload = data.subarray(offset, offset + len);

            if (type === MSG_SYNC_STEP2) {
                Y.applyUpdate(this.doc, payload, 'remote');
                this.synced = true;
                
                // Check for marker in the document text
                // Note: Scanning full text every update is slow for huge docs, 
                // but fine for this benchmark.
                const text = this.ytext.toString();
                // Regex to find our markers: {{MARKER:ID:TIMESTAMP}}
                const matches = text.match(/{{MARKER:(\d+):(\d+)}}/g);
                if (matches && this.onLatencyMeasure) {
                    matches.forEach(m => {
                        const parts = m.match(/{{MARKER:(\d+):(\d+)}}/);
                        if (parts) {
                            const id = parseInt(parts[1]);
                            const sentTime = parseInt(parts[2]);
                            this.onLatencyMeasure(id, sentTime);
                        }
                    });
                }
            }
        } catch (e) {
            // ignore parsing errors
        }
    }
}

// --- Orchestrator ---
async function runBenchmark() {
    const counts = [10, 50, 100, 200];
    const results = {};

    // Cleanup function
    const cleanup = async (clients) => {
        console.log("Cleaning up clients...");
        clients.forEach(c => c.disconnect());
        await new Promise(r => setTimeout(r, 1000));
    };

    for (const count of counts) {
        console.log(`\n--- Benchmarking with ${count} Clients ---`);
        const clients = [];
        
        // 1. Connect Clients
        for (let i = 0; i < count; i++) {
            const bot = new BenchBot(i);
            await bot.connect();
            clients.push(bot);
            if (i % 50 === 0) process.stdout.write('.');
        }
        console.log("\nAll clients connected.");
        await new Promise(r => setTimeout(r, 2000)); // Settle

        // 2. Prepare Measurement
        const measurements = [];
        const markerId = Math.floor(Math.random() * 100000);
        let receivedCount = 0;
        const expectedReceivers = count - 1; // Everyone except sender

        const donePromise = new Promise(resolve => {
            const checkDone = () => {
                if (receivedCount >= expectedReceivers) resolve();
            };
            
            // Setup listeners on all receivers
            for (let i = 1; i < count; i++) {
                clients[i].onLatencyMeasure = (id, sentTime) => {
                    if (id === markerId) {
                        const latency = Date.now() - sentTime;
                        measurements.push(latency);
                        receivedCount++;
                        // Remove listener to avoid double counting if re-sent
                        clients[i].onLatencyMeasure = null; 
                        checkDone();
                    }
                };
            }
            
            // Timeout safety
            setTimeout(() => {
                console.log(`Timeout! Received ${receivedCount}/${expectedReceivers}`);
                resolve();
            }, 10000); 
        });

        // 3. Writer sends update
        console.log("Broadcasting update...");
        const sentTime = clients[0].sendMarker(markerId);
        
        if (!sentTime) {
            console.error("Failed to send marker");
            await cleanup(clients);
            continue;
        }

        // 4. Wait for results
        await donePromise;

        // 5. Calc Stats
        if (measurements.length > 0) {
            const avg = measurements.reduce((a, b) => a + b, 0) / measurements.length;
            const max = Math.max(...measurements);
            const min = Math.min(...measurements);
            console.log(`Results for ${count} clients:`);
            console.log(`  Avg Latency: ${avg.toFixed(2)} ms`);
            console.log(`  Min Latency: ${min} ms`);
            console.log(`  Max Latency: ${max} ms`);
            results[count] = { avg, min, max };
        } else {
            console.log("No measurements received.");
        }

        await cleanup(clients);
        await new Promise(r => setTimeout(r, 2000)); // Cooldown
    }

    console.log("\n=== Final Summary ===");
    console.table(results);
}

runBenchmark();


