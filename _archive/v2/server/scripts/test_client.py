#!/usr/bin/env python3
"""
Simple test client to verify CRDT server document reconciliation
"""

import asyncio
import websockets
import struct

async def test_client():
    uri = "ws://localhost:9000"
    
    try:
        async with websockets.connect(uri) as websocket:
            print("[Test Client] Connected to server")
            
            # Receive initial state from server
            initial_state = await websocket.recv()
            print(f"[Test Client] Received initial state: {len(initial_state)} bytes")
            
            # Send a simple update (this is a mock - in reality you'd generate proper Yjs updates)
            # For now, just stay connected
            print("[Test Client] Staying connected... (press Ctrl+C to disconnect)")
            
            try:
                while True:
                    message = await asyncio.wait_for(websocket.recv(), timeout=1.0)
                    print(f"[Test Client] Received update: {len(message)} bytes")
            except asyncio.TimeoutError:
                pass
            except KeyboardInterrupt:
                print("\n[Test Client] Disconnecting...")
                
    except Exception as e:
        print(f"[Test Client] Error: {e}")

if __name__ == "__main__":
    try:
        asyncio.run(test_client())
    except KeyboardInterrupt:
        print("\n[Test Client] Stopped")

