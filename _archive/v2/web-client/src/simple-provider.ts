/**
 * Simple WebSocket provider for Yjs that sends raw updates
 * Compatible with our custom CRDT server
 */
import * as Y from 'yjs';

export class SimpleProvider {
  private ws: WebSocket | null = null;
  private doc: Y.Doc;
  private url: string;
  private connected: boolean = false;
  private updateHandler: ((update: Uint8Array, origin: any) => void) | null = null;

  constructor(url: string, doc: Y.Doc) {
    this.url = url;
    this.doc = doc;
    this.connect();
  }

  private connect() {
    console.log('[Provider] Connecting to', this.url);
    
    this.ws = new WebSocket(this.url);
    this.ws.binaryType = 'arraybuffer';

    this.ws.onopen = () => {
      console.log('[Provider] Connected');
      this.connected = true;
      this.emit('status', { status: 'connected' });
      
      // Send current document state to server
      const stateVector = Y.encodeStateVector(this.doc);
      console.log('[Provider] Sending state vector:', stateVector.length, 'bytes');
      this.ws?.send(stateVector);
    };

    this.ws.onmessage = (event) => {
      const update = new Uint8Array(event.data);
      console.log('[Provider] Received update:', update.length, 'bytes');
      
      // Apply update from server
      Y.applyUpdate(this.doc, update, this);
    };

    this.ws.onerror = (error) => {
      console.error('[Provider] WebSocket error:', error);
      this.emit('status', { status: 'error' });
    };

    this.ws.onclose = () => {
      console.log('[Provider] Disconnected');
      this.connected = false;
      this.emit('status', { status: 'disconnected' });
      
      // Attempt to reconnect after 1 second
      setTimeout(() => this.connect(), 1000);
    };

    // Listen for local document changes and send to server
    this.updateHandler = (update: Uint8Array, origin: any) => {
      // Don't send updates that came from the server (origin === this)
      if (origin !== this && this.connected && this.ws?.readyState === WebSocket.OPEN) {
        console.log('[Provider] Sending update:', update.length, 'bytes');
        this.ws.send(update);
      }
    };

    this.doc.on('update', this.updateHandler);
  }

  private statusListeners: Array<(event: { status: string }) => void> = [];

  on(event: 'status', callback: (event: { status: string }) => void) {
    if (event === 'status') {
      this.statusListeners.push(callback);
    }
  }

  private emit(event: 'status', data: { status: string }) {
    if (event === 'status') {
      this.statusListeners.forEach(cb => cb(data));
    }
  }

  destroy() {
    if (this.updateHandler) {
      this.doc.off('update', this.updateHandler);
    }
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }
}

