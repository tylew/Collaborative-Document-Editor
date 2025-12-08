import { useEffect, useState, useRef } from 'react';
import * as Y from 'yjs';
import { QuillBinding } from 'y-quill';
import Quill from 'quill';
// @ts-ignore - quill-cursors provides its own types
import QuillCursors from 'quill-cursors';
import 'quill/dist/quill.snow.css';
import { YWebSocketProvider, AwarenessState } from './lib/y-websocket-provider';
import { InfoModal } from './components/InfoModal';
import { JoinModal } from './components/JoinModal';

const userColors = [
  '#30bced', '#6eeb83', '#ffbc42', '#ecd444', '#ee6352',
  '#9ac2c9', '#8acb88', '#1be7ff', '#ff6b6b', '#4ecdc4',
];

// Register cursor module
Quill.register('modules/cursors', QuillCursors);
// Removed generateUsername function as it's no longer needed for initialization

export default function App() {
  const [username, setUsername] = useState('');
  const [isJoined, setIsJoined] = useState(false);
  const [status, setStatus] = useState<'connecting' | 'connected' | 'disconnected' | 'error'>('connecting');
  const [synced, setSynced] = useState(false);
  const [awarenessMap, setAwarenessMap] = useState<Map<number, AwarenessState>>(new Map());
  const [isModalOpen, setIsModalOpen] = useState(false);

  const myColor = useRef(userColors[Math.floor(Math.random() * userColors.length)]);
  const ydocRef = useRef<Y.Doc | null>(null);
  const ytextRef = useRef<Y.Text | null>(null);
  const providerRef = useRef<YWebSocketProvider | null>(null);
  const editorRef = useRef<HTMLDivElement | null>(null);
  const quillInstanceRef = useRef<Quill | null>(null);
  const bindingRef = useRef<QuillBinding | null>(null);
  const initializedRef = useRef(false);
  const lastCursorSendRef = useRef<number>(0);
  const cursorsRef = useRef<any>(null);
  // Add a ref to track the current username for event listeners
  const usernameRef = useRef(username);

  // Keep usernameRef in sync with username state
  useEffect(() => {
    usernameRef.current = username;
  }, [username]);




  useEffect(() => {
    // Only initialize when user has joined
    if (!isJoined) return;

    // Prevent double initialization
    if (initializedRef.current) {
      console.log('[App] Already initialized, skipping');
      return;
    }
    initializedRef.current = true;
    
    console.log('[App] Initializing...');
    
    // Initialize Yjs document
    const ydoc = new Y.Doc();
    const ytext = ydoc.getText('quill'); // Match server shared type
    ydocRef.current = ydoc;
    ytextRef.current = ytext;

    // Connect to server
    const wsUrl = 'ws://100.104.86.121:9000';
    console.log('[App] Connecting to', wsUrl);

    // Use custom provider that works with our server
    const provider = new YWebSocketProvider(wsUrl, ydoc);
    providerRef.current = provider;

    // Status updates
    provider.on('status', (event: any) => {
      setStatus(event.status);
    });

    provider.on('synced', () => {
      console.log('[App] Provider synced event received');
      setSynced(true);
    });

    provider.on('awareness', (states: Map<number, AwarenessState>) => {
      setAwarenessMap(new Map(states));
    });

    // Set initial awareness (no cursor yet)
    provider.setLocalAwareness({
      user: { name: username, color: myColor.current }
    });

    // Initialize Quill editor with y-quill binding after provider is ready
    const initEditor = () => {
      if (editorRef.current && !quillInstanceRef.current && ydoc && ytext) {
        try {
          // Make sure the element is empty before initializing Quill
          editorRef.current.innerHTML = '';

          const quill = new Quill(editorRef.current, {
            theme: 'snow',
            modules: {
              cursors: {
                hideDelay: 5000,
                hideSpeed: 0,
                transformOnTextChange: true,
                selectionChangeSource: null,
              },
              toolbar: [
                [{ 'header': [1, 2, false] }],
                ['bold', 'italic', 'underline', 'strike'],
                [{ 'list': 'ordered'}, { 'list': 'bullet' }],
                ['link'],
                ['clean']
              ]
            },
            placeholder: 'Start collaborating...'
          });

          quillInstanceRef.current = quill;

          // Cursor module
          const cursors = quill.getModule('cursors') as any;
          cursorsRef.current = cursors;
          cursors.clearCursors();

          // Bind Yjs to Quill - automatic synchronization
          const binding = new QuillBinding(ytext, quill);
          bindingRef.current = binding;

          console.log('[App] Yjs-Quill binding established with standard provider');

          const sendCursor = (range: any) => {
            if (!providerRef.current) return;
            const now = Date.now();
            // Throttle cursor updates to ~50ms
            if (now - lastCursorSendRef.current < 50) return;
            lastCursorSendRef.current = now;

            const cursor =
              range && typeof range.index === 'number'
                ? { index: range.index, length: range.length ?? 0 }
                : undefined;

            providerRef.current.setLocalAwareness({
              user: { name: usernameRef.current, color: myColor.current },
              cursor,
            });
          };

          // Track selection changes to send awareness cursor
          quill.on('selection-change', (range, _oldRange, _source) => {
            // Always send; for remote-induced changes, source can be 'api'
            sendCursor(range);
          });

          // Also send on text changes (typing moves cursor but may not fire selection-change)
          quill.on('text-change', (_delta, _oldDelta, src) => {
            if (src === 'user') {
              const range = quill.getSelection();
              sendCursor(range);
            }
          });
        } catch (error) {
          console.error('[App] Failed to initialize Quill editor:', error);
        }
      }
    };

    // Initialize editor after a short delay to ensure DOM is ready
    setTimeout(initEditor, 100);

    // Cleanup
    return () => {
      console.log('[App] Cleanup starting...');
      
      if (bindingRef.current) {
        bindingRef.current.destroy();
        bindingRef.current = null;
      }
      
      if (quillInstanceRef.current) {
        // Quill doesn't have a destroy method, so we need to clean up manually
        quillInstanceRef.current = null;
      }
      
      // Clear the editor container to prevent duplicate editors
      if (editorRef.current) {
        editorRef.current.innerHTML = '';
      }
      
      provider.destroy();
      
      // Clean up YDoc
      ydoc.destroy();
      ydocRef.current = null;
      ytextRef.current = null;
      providerRef.current = null;
      
      // Reset initialization flag for potential re-mount
      initializedRef.current = false;
      
      console.log('[App] Cleanup complete');
    };
  }, [isJoined]);

  // Update awareness when username changes
  useEffect(() => {
    if (providerRef.current) {
      providerRef.current.setLocalAwareness({
        user: { name: username, color: myColor.current },
      });
    }
  }, [username]);

  // Update remote cursors when awareness changes
  useEffect(() => {
    const quill = quillInstanceRef.current;
    const cursors = cursorsRef.current;
    const selfId = ydocRef.current?.clientID;
    if (!quill || !cursors || selfId === undefined) return;

    cursors.clearCursors();

    awarenessMap.forEach((state, clientId) => {
      if (clientId === selfId) return;
      if (!state.cursor) return;
      const color = state.user?.color || '#30bced';
      const name = state.user?.name || `User ${clientId}`;
      const cursorId = String(clientId);

      cursors.createCursor(cursorId, name, color);
      cursors.moveCursor(cursorId, state.cursor);
    });
  }, [awarenessMap]);

  const getStatusColor = () => {
    if (status === 'connected' && synced) return 'bg-green-500';
    if (status === 'connected') return 'bg-yellow-500 animate-pulse';
    if (status === 'connecting') return 'bg-yellow-500 animate-pulse';
    if (status === 'error') return 'bg-red-500';
    return 'bg-gray-500';
  };

  const getStatusText = () => {
    if (status === 'connected' && synced) return 'Connected & Synced';
    if (status === 'connected') return 'Connected (syncing...)';
    return status.charAt(0).toUpperCase() + status.slice(1);
  };

  const handleJoin = (name: string) => {
    setUsername(name);
    setIsJoined(true);
  };

  const selfId = ydocRef.current?.clientID;
  // Get all other peers
  const otherPeers = [...awarenessMap.entries()].filter(([clientId]) => clientId !== selfId);
  
  // Sort peers: prioritize those with cursors
  const sortedPeers = otherPeers.sort(([_idA, stateA], [_idB, stateB]) => {
    const aHasCursor = !!stateA.cursor;
    const bHasCursor = !!stateB.cursor;
    if (aHasCursor && !bHasCursor) return -1;
    if (!aHasCursor && bHasCursor) return 1;
    return 0;
  });

  const MAX_DISPLAY_PEERS = 5;
  const displayPeers = sortedPeers.slice(0, MAX_DISPLAY_PEERS);
  const remainingPeersCount = sortedPeers.length - MAX_DISPLAY_PEERS;

  if (!isJoined) {
    return <JoinModal onJoin={handleJoin} />;
  }

  return (
    <div className="h-screen bg-gray-50 flex flex-col overflow-hidden">
      {/* Header */}
      <header className="bg-white border-b border-gray-200 flex-shrink-0">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex justify-between items-center h-16">
            <div className="flex items-center space-x-3">
              <h1 className="text-xl font-bold text-gray-900">
                Collaborative Document Editor
              </h1>
              <div className="flex items-center space-x-2">
                <div className={`w-2 h-2 rounded-full ${getStatusColor()}`} />
                <span className="text-sm text-gray-600">
                  {getStatusText()}
                </span>
              </div>
            </div>

            <div className="flex items-center space-x-4">
              <div className="flex items-center space-x-2">
                <div
                  className="w-3 h-3 rounded-full"
                  style={{ background: myColor.current }}
                />
                <input
                  type="text"
                  value={username}
                  onChange={(e) => setUsername(e.target.value)}
                  className="px-3 py-1 text-sm border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                  placeholder="Your name"
                />
              </div>
            </div>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <main className="flex-1 w-full max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-2 min-h-0">
        <div className="grid grid-cols-1 lg:grid-cols-4 gap-6 h-full">
          {/* Editor */}
          <div className="lg:col-span-3 h-full">
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden h-full flex flex-col">
              <div
                ref={editorRef}
                className="flex-1 overflow-hidden"
              />
            </div>

          </div>

          {/* Sidebar */}
          <div className="lg:col-span-1 h-full min-h-0">
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4 max-h-full overflow-y-auto">
              <h2 className="text-lg font-semibold text-gray-900 mb-4">
                Connected Users
              </h2>
              <div className="space-y-2">
                <div className="flex items-center space-x-2 p-2 bg-gray-50 rounded">
                  <div
                    className="w-3 h-3 rounded-full"
                    style={{ background: myColor.current }}
                  />
                  <span className="text-sm text-gray-900">
                    {username} <span className="text-gray-500">(You)</span>
                  </span>
                </div>
                {displayPeers.map(([clientId, state]) => (
                    <div key={clientId} className="flex items-center space-x-2 p-2 bg-gray-50 rounded">
                      <div
                        className="w-3 h-3 rounded-full"
                        style={{ background: state.user.color }}
                      />
                      <span className="text-sm text-gray-900">
                        {state.user.name || `User ${clientId}`}
                      </span>
                      {state.cursor && (
                        <span className="text-xs text-gray-500">
                          (cursor: {state.cursor.index}, len {state.cursor.length})
                        </span>
                      )}
                    </div>
                  ))}
                
                {remainingPeersCount > 0 && (
                  <div className="flex items-center space-x-2 p-2 pl-7">
                    <span className="text-sm text-gray-500 italic">
                      ...and {remainingPeersCount} other peer{remainingPeersCount > 1 ? 's' : ''}
                    </span>
                  </div>
                )}

                {sortedPeers.length === 0 && (
                  <div className="flex items-center space-x-2 p-2 opacity-50">
                    <div className="w-3 h-3 rounded-full bg-gray-400" />
                    <span className="text-sm text-gray-500">
                      No other peers connected
                    </span>
                  </div>
                )}
              </div>

              <div className="mt-6 pt-6 border-t border-gray-200">
                <h3 className="text-sm font-medium text-gray-700 mb-2">
                  About
                </h3>
                <p className="text-xs text-gray-500 mb-3">
                  Real-time collaborative document editor powered by Yjs CRDT.
                  Changes sync automatically with proper protocol implementation.
                </p>
                <button
                  onClick={() => setIsModalOpen(true)}
                  className="text-xs text-blue-600 hover:text-blue-800 font-medium hover:underline focus:outline-none"
                >
                  More Information
                </button>
              </div>
             
            </div>
          </div>
        </div>

      {/* Footer */}
      {/* <footer className="mt-8 py-6 border-t border-gray-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <p className="text-center text-sm text-gray-500">
            Tyler Lewis 2025 • Version 2.0.0 • <a href="https://github.com/tylew/Collaborative-Document-Editor" className="text-blue-500">GitHub</a>
            </p>
          </div>
        </footer> */}
        </main>
      
      <InfoModal isOpen={isModalOpen} onClose={() => setIsModalOpen(false)} />
    </div>
  );
}
