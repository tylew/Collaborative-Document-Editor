
const WebSocket = require('ws');
const Y = require('yjs');

const SERVER_URL = 'ws://localhost:9000';

// Protocol Constants
const MSG_SYNC_STEP1 = 0;
const MSG_SYNC_STEP2 = 1;
const MSG_AWARENESS = 2;

// --- VarInt Helper Functions (Ported from protocol.ts) ---
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

// --- Custom Client Implementation ---
class BotClient {
    constructor(id) {
        this.id = id;
        this.name = `JS-Bot-${id}`;
        this.color = '#' + Math.floor(Math.random() * 16777215).toString(16);
        this.doc = new Y.Doc();
        this.ytext = this.doc.getText('quill');
        this.ws = null;
        this.synced = false;
        
        this.doc.on('update', (update, origin) => {
            // 'remote' = update from server
            // 'local' = update from our simulation
            if (origin === 'local') {
                this.sendUpdate(update);
            }
        });
    }

    connect() {
        this.ws = new WebSocket(SERVER_URL);
        this.ws.binaryType = 'arraybuffer';

        this.ws.on('open', () => {
            console.log(`[${this.name}] Connected`);
            this.startSync();
            this.sendAwareness();
            this.startSimulation();
        });

        this.ws.on('message', (data) => this.handleMessage(new Uint8Array(data)));
        
        this.ws.on('error', (err) => console.error(`[${this.name}] Error:`, err.message));
    }

    startSync() {
        // Send SYNC_STEP1 (Empty Vector)
        const vector = Y.encodeStateVector(this.doc);
        const len = encodeVarUint(vector.length);
        const msg = new Uint8Array(1 + len.length + vector.length);
        msg[0] = MSG_SYNC_STEP1;
        msg.set(len, 1);
        msg.set(vector, 1 + len.length);
        this.ws.send(msg);
    }

    sendUpdate(update) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;
        
        const len = encodeVarUint(update.length);
        const msg = new Uint8Array(1 + len.length + update.length);
        msg[0] = MSG_SYNC_STEP2;
        msg.set(len, 1);
        msg.set(update, 1 + len.length);
        this.ws.send(msg);
    }

    sendAwareness(cursor = null) {
        if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return;

        const state = {
            user: { name: this.name, color: this.color }
        };
        if (cursor) state.cursor = cursor;

        const jsonBytes = Buffer.from(JSON.stringify(state), 'utf8');
        
        // ClientID is not strictly needed in payload if server sets it, 
        // but your protocol.ts encodes it. The server actually *overwrites* it
        // based on the connection usually, but let's follow the protocol format:
        // [MSG_AWARENESS][TotalLen][ClientID][JSONLen][JSON]
        
        // We'll simulate a Client ID (just a random number for the protocol field)
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

    handleMessage(data) {
        try {
            const type = data[0];
            const [len, offset] = decodeVarUint(data, 1);
            const payload = data.subarray(offset, offset + len);

            if (type === MSG_SYNC_STEP2) {
                Y.applyUpdate(this.doc, payload, 'remote'); // Origin = 'remote'
                if (!this.synced) {
                    this.synced = true;
                    console.log(`[${this.name}] Initial Sync Complete`);
                }
            }
            // We can ignore STEP1 (server asking for data) and AWARENESS (other users) for this bot
        } catch (e) {
            console.error(`[${this.name}] Parse Error:`, e);
        }
    }

    startSimulation() {
        this.typingStarted = false;
        
        setInterval(() => {
            // 1. Move Cursor (Always active)
            const index = Math.floor(Math.random() * (this.ytext.length + 1));
            this.sendAwareness({ index, length: 0 });

            // 2. Insert Text (Once per bot, char by char)
            if (!this.typingStarted && Math.random() < 0.1) {
                this.typingStarted = true;
                this.messageToType = `[Bot-${this.id} was here] `;
                this.charsTyped = 0;
                this.typeNextChar();
            }
        }, 1000 + Math.random() * 2000);
    }

    typeNextChar() {
        if (this.charsTyped >= this.messageToType.length) {
            console.log(`[${this.name}] Finished typing`);
            return;
        }

        this.doc.transact(() => {
            // Recalculate position every time because other bots might have shifted it
            const targetLineIndex = this.id % 15;
            const content = this.ytext.toString();
            let newlines = [];
            for (let i = 0; i < content.length; i++) {
                if (content[i] === '\n') newlines.push(i);
            }

            if (newlines.length <= targetLineIndex) {
                const needed = targetLineIndex - newlines.length + 1;
                this.ytext.insert(this.ytext.length, "\n".repeat(needed));
                // Quick rescan
                const newContent = this.ytext.toString();
                newlines = [];
                for (let i = 0; i < newContent.length; i++) {
                    if (newContent[i] === '\n') newlines.push(i);
                }
            }

            let startOfLine = 0;
            if (targetLineIndex > 0 && newlines.length >= targetLineIndex) {
                startOfLine = newlines[targetLineIndex - 1] + 1;
            }

            // Insert at end of current message progress
            const char = this.messageToType[this.charsTyped];
            this.ytext.insert(startOfLine + this.charsTyped, char);
            
            this.sendAwareness({ index: startOfLine + this.charsTyped + 1, length: 0 });

        }, 'local');

        this.charsTyped++;
        
        // Schedule next char
        setTimeout(() => this.typeNextChar(), 100 + Math.random() * 100);
    }
}

// Start Bots
(async () => {
    console.log("Starting Custom Protocol JS Bots in 10 seconds...");
    await new Promise(r => setTimeout(r, 10000));
    console.log("Launching bots now...");
    
    for (let i = 0; i < 10; i++) {
        const bot = new BotClient(i + 100);
        bot.connect();
        await new Promise(r => setTimeout(r, 200));
    }
})();
