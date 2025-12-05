/**
 * y-websocket protocol implementation
 *
 * Message format: [messageType: uint8][payload]
 *
 * SYNC_STEP1 (0): [varuint: length][state vector]
 * SYNC_STEP2 (1): [varuint: length][update]
 * AWARENESS (2): [varuint: length][awareness data] (not implemented)
 */

export enum MessageType {
  SYNC_STEP1 = 0,
  SYNC_STEP2 = 1,
  AWARENESS = 2,
}

/**
 * Encode variable-length unsigned integer (varint)
 * Uses 7 bits per byte, 8th bit = continuation flag
 */
export function encodeVarUint(num: number): Uint8Array {
  const result: number[] = [];

  while (num >= 0x80) {
    result.push((num & 0x7f) | 0x80);
    num >>>= 7;
  }
  result.push(num & 0x7f);

  return new Uint8Array(result);
}

/**
 * Decode variable-length unsigned integer
 * Returns [value, bytesRead]
 */
export function decodeVarUint(data: Uint8Array): [number, number] {
  let result = 0;
  let shift = 0;
  let pos = 0;

  while (pos < data.length) {
    const byte = data[pos++];
    result |= (byte & 0x7f) << shift;

    if ((byte & 0x80) === 0) {
      // Last byte (no continuation bit)
      return [result, pos];
    }

    shift += 7;
    if (shift >= 32) {
      throw new Error('Varint overflow');
    }
  }

  throw new Error('Incomplete varint');
}

/**
 * Encode SYNC_STEP1 message
 * Format: [type=0][varuint: sv_len][state_vector]
 */
export function encodeSyncStep1(stateVector: Uint8Array): Uint8Array {
  const varint = encodeVarUint(stateVector.length);
  const result = new Uint8Array(1 + varint.length + stateVector.length);

  result[0] = MessageType.SYNC_STEP1;
  result.set(varint, 1);
  result.set(stateVector, 1 + varint.length);

  return result;
}

/**
 * Encode SYNC_STEP2 message
 * Format: [type=1][varuint: update_len][update]
 */
export function encodeSyncStep2(update: Uint8Array): Uint8Array {
  const varint = encodeVarUint(update.length);
  const result = new Uint8Array(1 + varint.length + update.length);

  result[0] = MessageType.SYNC_STEP2;
  result.set(varint, 1);
  result.set(update, 1 + varint.length);

  return result;
}

/**
 * Decode message
 * Returns { type, payload }
 */
export function decodeMessage(data: Uint8Array): {
  type: MessageType;
  payload: Uint8Array;
} {
  if (data.length < 2) {
    throw new Error('Message too short');
  }

  const type = data[0] as MessageType;

  if (type !== MessageType.SYNC_STEP1 && type !== MessageType.SYNC_STEP2 && type !== MessageType.AWARENESS) {
    throw new Error(`Unknown message type: ${type}`);
  }

  // Decode varint length
  const [payloadLen, varintBytes] = decodeVarUint(data.subarray(1));

  const payloadOffset = 1 + varintBytes;
  const remaining = data.length - payloadOffset;

  if (remaining < payloadLen) {
    throw new Error(`Incomplete message: expected ${payloadLen} bytes, got ${remaining}`);
  }

  const payload = data.subarray(payloadOffset, payloadOffset + payloadLen);

  return { type, payload };
}
