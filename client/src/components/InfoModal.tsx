import React from 'react';

interface InfoModalProps {
  isOpen: boolean;
  onClose: () => void;
}

export const InfoModal: React.FC<InfoModalProps> = ({ isOpen, onClose }) => {
  if (!isOpen) return null;

  return (
    <div 
      className="fixed inset-0 z-50 flex items-center justify-center bg-black bg-opacity-50"
      onClick={onClose}
    >
      <div 
        className="bg-white rounded-lg shadow-xl p-6 max-w-2xl w-full max-h-[90vh] overflow-y-auto m-4"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="space-y-6">
          <div>
            <h3 className="text-lg font-medium text-gray-900 mb-2">
              Client Implementation
            </h3>
            <div className="text-sm text-gray-600 space-y-2">
              <p>
                <strong>CRDT:</strong>{' '}
                <a
                  href="https://yjs.dev"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-blue-600 hover:text-blue-800 underline"
                >
                  Yjs
                </a>{' '}
                JavaScript library for conflict-free replicated data types
              </p>
              <p>
                <strong>WebSocket:</strong>{' '}
                <a
                  href="https://developer.mozilla.org/en-US/docs/Web/API/WebSocket"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-blue-600 hover:text-blue-800 underline"
                >
                  WebSocket API
                </a>{' '}
                for real-time bidirectional communication
              </p>
            </div>
          </div>

          <div className="border-t border-gray-200 pt-6">
            <h3 className="text-lg font-medium text-gray-900 mb-2">
              Server Implementation
            </h3>
            <div className="text-sm text-gray-600 space-y-2">
              <p>
                <strong>CRDT:</strong>{' '}
                <a
                  href="https://github.com/y-crdt/y-crdt"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-blue-600 hover:text-blue-800 underline"
                >
                  Yrs
                </a>{' '}
                Rust Yrs with C FFI bindings for high-performance CRDT operations
              </p>
              <p>
                <strong>WebSocket:</strong>{' '}
                <a
                  href="https://libwebsockets.org"
                  target="_blank"
                  rel="noopener noreferrer"
                  className="text-blue-600 hover:text-blue-800 underline"
                >
                  libwebsockets
                </a>{' '}
                C library for WebSocket server implementation
              </p>
              <p>
                <strong>Threading:</strong> OpenMP for thread-safe peer management and concurrent operations
              </p>
            </div>
          </div>

          <div className="border-t border-gray-200 pt-6">
            <h3 className="text-lg font-medium text-gray-900 mb-2">
              Protocol Details
            </h3>
            <div className="text-sm text-gray-600 space-y-2">
              <p>
                Custom WebSocket protocol with varint encoding for efficient CRDT synchronization.
                All messages follow the format: <code>[messageType: uint8][varuint: length][payload]</code>
              </p>

              <div className="space-y-1 mt-2">
                <p className="font-medium">Message Types:</p>
                <ul className="ml-4 space-y-1 list-disc">
                  <li>
                    <strong>SYNC_STEP1 (0):</strong> State vector exchange - Client sends current state vector,
                    server responds with initial document state for differential sync
                  </li>
                  <li>
                    <strong>SYNC_STEP2 (1):</strong> Update data - Client sends document changes,
                    server broadcasts to all connected peers
                  </li>
                  <li>
                    <strong>AWARENESS (2):</strong> Presence/cursor information - Clients share user presence and
                    cursor positions
                  </li>
                </ul>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

