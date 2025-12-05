/**
 * Y-WebSocket Provider
 *
 * Implements the y-websocket protocol for syncing Yjs documents over WebSocket.
 * Uses SYNC_STEP1/SYNC_STEP2 messages for efficient state synchronization.
 */

import * as Y from 'yjs';
import { MessageType, encodeSyncStep1, encodeSyncStep2, decodeMessage } from './protocol';

type StatusEvent = {
  status: 'connecting' | 'connected' | 'disconnected' | 'error';
};

type EventCallback = (event: any) => void;

export class YWebSocketProvider {
  private doc: Y.Doc;
  private ws: WebSocket | null = null;
  private url: string;
  private connected: boolean = false;
  private synced: boolean = false;
  private reconnectAttempts: number = 0;
  private maxReconnectAttempts: number = 10;
  private reconnectDelay: number = 1000;

  private eventListeners: Map<string, EventCallback[]> = new Map();
  private updateHandler: ((update: Uint8Array, origin: any) => void) | null = null;

  constructor(url: string, doc: Y.Doc) {
    this.doc = doc;
    this.url = url;
    this.connect();
  }

  private connect() {
    console.log('[Provider] Connecting to', this.url);
    this.emit('status', { status: 'connecting' });

    this.ws = new WebSocket(this.url);
    this.ws.binaryType = 'arraybuffer';

    this.ws.onopen = () => this.handleOpen();
    this.ws.onmessage = (event) => this.handleMessage(event);
    this.ws.onerror = (error) => this.handleError(error);
    this.ws.onclose = () => this.handleClose();

    // Listen for local document updates
    this.updateHandler = (update: Uint8Array, origin: any) => {
      // Don't send updates that came from the server (origin === this)
      if (origin !== this && this.connected && this.synced) {
        this.sendUpdate(update);
      }
    };

    this.doc.on('update', this.updateHandler);
  }

  private handleOpen() {
    console.log('[Provider] Connected');
    this.connected = true;
    this.reconnectAttempts = 0;
    this.emit('status', { status: 'connected' });

    // Send SYNC_STEP1 with our state vector
    const stateVector = Y.encodeStateVector(this.doc);
    const message = encodeSyncStep1(stateVector);

    console.log('[Provider] Sending SYNC_STEP1 (state vector:', stateVector.length, 'bytes)');
    this.ws!.send(message);
  }

  private handleMessage(event: MessageEvent) {
    try {
      const data = new Uint8Array(event.data);
      const { type, payload } = decodeMessage(data);

      if (type === MessageType.SYNC_STEP1) {
        console.log('[Provider] Received SYNC_STEP1');

        // Server asking for our state (shouldn't happen with current impl)
        const stateVector = payload;
        const diff = Y.encodeStateAsUpdate(this.doc, stateVector);
        const message = encodeSyncStep2(diff);

        console.log('[Provider] Sending SYNC_STEP2 (diff:', diff.length, 'bytes)');
        this.ws!.send(message);
      } else if (type === MessageType.SYNC_STEP2) {
        console.log('[Provider] Received SYNC_STEP2 (update:', payload.length, 'bytes)');

        // Server sending update - apply to document
        Y.applyUpdate(this.doc, payload, this); // origin=this prevents echo

        if (!this.synced) {
          this.synced = true;
          this.emit('synced', {});
          console.log('[Provider] Document synced');
        }
      } else if (type === MessageType.AWARENESS) {
        // Awareness not implemented
        console.log('[Provider] Received AWARENESS (ignored)');
      }
    } catch (error) {
      console.error('[Provider] Failed to handle message:', error);
    }
  }

  private handleError(error: Event) {
    console.error('[Provider] WebSocket error:', error);
    this.emit('status', { status: 'error' });
  }

  private handleClose() {
    console.log('[Provider] Disconnected');
    this.connected = false;
    this.synced = false;
    this.emit('status', { status: 'disconnected' });

    // Attempt to reconnect
    if (this.reconnectAttempts < this.maxReconnectAttempts) {
      this.reconnectAttempts++;
      const delay = this.reconnectDelay * Math.min(this.reconnectAttempts, 5);

      console.log(
        `[Provider] Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`
      );

      setTimeout(() => {
        if (this.ws) {
          this.connect();
        }
      }, delay);
    } else {
      console.error('[Provider] Max reconnect attempts reached');
    }
  }

  private sendUpdate(update: Uint8Array) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      console.warn('[Provider] Cannot send update: not connected');
      return;
    }

    const message = encodeSyncStep2(update);
    console.log('[Provider] Sending update:', update.length, 'bytes');
    this.ws.send(message);
  }

  public on(event: 'status' | 'synced', callback: EventCallback) {
    if (!this.eventListeners.has(event)) {
      this.eventListeners.set(event, []);
    }
    this.eventListeners.get(event)!.push(callback);
  }

  private emit(event: string, data: any) {
    const callbacks = this.eventListeners.get(event);
    if (callbacks) {
      callbacks.forEach((cb) => cb(data));
    }
  }

  public destroy() {
    console.log('[Provider] Destroying');

    if (this.updateHandler) {
      this.doc.off('update', this.updateHandler);
      this.updateHandler = null;
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }

    this.eventListeners.clear();
  }
}
