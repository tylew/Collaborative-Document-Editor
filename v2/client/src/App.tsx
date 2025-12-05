import { useEffect, useState, useRef } from 'react';
import * as Y from 'yjs';
import ReactQuill from 'react-quill';
import 'react-quill/dist/quill.snow.css';
import { YWebSocketProvider } from './lib/y-websocket-provider';

const userColors = [
  '#30bced', '#6eeb83', '#ffbc42', '#ecd444', '#ee6352',
  '#9ac2c9', '#8acb88', '#1be7ff', '#ff6b6b', '#4ecdc4',
];

function generateUsername(): string {
  const adjectives = ['Quick', 'Bright', 'Swift', 'Happy', 'Clever', 'Brave', 'Calm', 'Bold', 'Smart'];
  const nouns = ['Panda', 'Eagle', 'Tiger', 'Dolphin', 'Fox', 'Wolf', 'Bear', 'Lion', 'Hawk'];
  const adj = adjectives[Math.floor(Math.random() * adjectives.length)];
  const noun = nouns[Math.floor(Math.random() * nouns.length)];
  return `${adj}${noun}${Math.floor(Math.random() * 100)}`;
}

export default function App() {
  const [username, setUsername] = useState(generateUsername());
  const [status, setStatus] = useState<'connecting' | 'connected' | 'disconnected' | 'error'>('connecting');
  const [synced, setSynced] = useState(false);
  const [editorValue, setEditorValue] = useState('');

  const myColor = useRef(userColors[Math.floor(Math.random() * userColors.length)]);
  const ydocRef = useRef<Y.Doc | null>(null);
  const ytextRef = useRef<Y.Text | null>(null);
  const providerRef = useRef<YWebSocketProvider | null>(null);
  const quillRef = useRef<ReactQuill | null>(null);
  const isRemoteChange = useRef(false);

  useEffect(() => {
    // Initialize Yjs document
    const ydoc = new Y.Doc();
    const ytext = ydoc.getText('document'); // MUST match server shared type
    ydocRef.current = ydoc;
    ytextRef.current = ytext;

    // Connect to server
    const wsUrl = import.meta.env.VITE_WS_URL || 'ws://localhost:9000';
    console.log('[App] Connecting to', wsUrl);

    const provider = new YWebSocketProvider(wsUrl, ydoc);
    providerRef.current = provider;

    // Status updates
    provider.on('status', (event) => {
      setStatus(event.status);
    });

    provider.on('synced', () => {
      setSynced(true);
    });

    // Yjs -> React: Apply remote changes to editor
    const ytextObserver = () => {
      if (isRemoteChange.current) return;

      isRemoteChange.current = true;
      const newText = ytext.toString();
      setEditorValue(newText);
      isRemoteChange.current = false;
    };

    ytext.observe(ytextObserver);

    // Cleanup
    return () => {
      ytext.unobserve(ytextObserver);
      provider.destroy();
    };
  }, []);

  const handleEditorChange = (content: string, _delta: any, _source: any, editor: any) => {
    if (isRemoteChange.current) return;

    const ytext = ytextRef.current;
    const ydoc = ydocRef.current;
    if (!ytext || !ydoc) return;

    isRemoteChange.current = true;

    // Get Quill delta
    const delta = editor.getContents();

    // Simple approach: sync plain text content
    const currentText = ytext.toString();
    if (currentText !== content) {
      ydoc.transact(() => {
        ytext.delete(0, currentText.length);
        ytext.insert(0, content);
      });
    }

    isRemoteChange.current = false;
  };

  const modules = {
    toolbar: [
      [{ header: [1, 2, false] }],
      ['bold', 'italic', 'underline'],
      [{ list: 'ordered' }, { list: 'bullet' }],
      ['blockquote', 'code-block'],
      ['link'],
      ['clean'],
    ],
    history: {
      userOnly: true,
    },
  };

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

  return (
    <div className="min-h-screen bg-gray-50">
      {/* Header */}
      <header className="bg-white border-b border-gray-200 sticky top-0 z-10">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="flex justify-between items-center h-16">
            <div className="flex items-center space-x-3">
              <h1 className="text-xl font-bold text-gray-900">
                Collaborative Editor v2
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
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        <div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
          {/* Editor */}
          <div className="lg:col-span-3">
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 overflow-hidden">
              <ReactQuill
                ref={quillRef}
                theme="snow"
                value={editorValue}
                onChange={handleEditorChange}
                modules={modules}
                placeholder="Start collaborating..."
                className="h-[600px]"
              />
            </div>
          </div>

          {/* Sidebar */}
          <div className="lg:col-span-1">
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
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
                <div className="flex items-center space-x-2 p-2 opacity-50">
                  <div className="w-3 h-3 rounded-full bg-gray-400" />
                  <span className="text-sm text-gray-500">
                    Other peers (no awareness)
                  </span>
                </div>
              </div>

              <div className="mt-6 pt-6 border-t border-gray-200">
                <h3 className="text-sm font-medium text-gray-700 mb-2">
                  Protocol Info
                </h3>
                <div className="text-xs text-gray-500 space-y-1">
                  <p>Protocol: y-websocket v2</p>
                  <p>CRDT: Yjs (libyrs server)</p>
                  <p>Shared type: "document"</p>
                  <p>Sync: SYNC_STEP1/STEP2</p>
                </div>
              </div>

              <div className="mt-6 pt-6 border-t border-gray-200">
                <h3 className="text-sm font-medium text-gray-700 mb-2">
                  About
                </h3>
                <p className="text-xs text-gray-500">
                  Real-time collaborative document editor powered by Yjs CRDT.
                  Changes sync automatically with proper protocol implementation.
                </p>
              </div>
            </div>
          </div>
        </div>
      </main>

      {/* Footer */}
      <footer className="mt-8 py-6 border-t border-gray-200">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <p className="text-center text-sm text-gray-500">
            V2 Implementation • Correct WebSocket Protocol • Text-only sync
          </p>
        </div>
      </footer>
    </div>
  );
}
