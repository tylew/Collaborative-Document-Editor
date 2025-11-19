import * as Y from 'yjs';
import Quill from 'quill';
import { SimpleProvider } from './simple-provider';

// Note: Cursors removed - server doesn't handle awareness

// User colors pool
const userColors = [
  '#30bced',
  '#6eeb83',
  '#ffbc42',
  '#ecd444',
  '#ee6352',
  '#9ac2c9',
  '#8acb88',
  '#1be7ff',
  '#ff6b6b',
  '#4ecdc4',
];

// Pick a random color for this user
const myColor = userColors[Math.floor(Math.random() * userColors.length)];

// Generate a random username
function generateUsername(): string {
  const adjectives = ['Quick', 'Bright', 'Swift', 'Happy', 'Clever', 'Brave', 'Calm', 'Bold', 'Smart'];
  const nouns = ['Panda', 'Eagle', 'Tiger', 'Dolphin', 'Fox', 'Wolf', 'Bear', 'Lion', 'Quat'];
  const adj = adjectives[Math.floor(Math.random() * adjectives.length)];
  const noun = nouns[Math.floor(Math.random() * nouns.length)];
  return `${adj}${noun}${Math.floor(Math.random() * 100)}`;
}

// Initialize Quill editor (no cursors - server doesn't support awareness)
const quill = new Quill('#editor', {
  modules: {
    toolbar: [
      [{ header: [1, 2, false] }],
      ['bold', 'italic', 'underline'],
      [{ list: 'ordered' }, { list: 'bullet' }],
      ['blockquote', 'code-block'],
      ['link', 'image'],
      ['clean'],
    ],
    history: {
      // Local undo shouldn't undo changes from remote users
      userOnly: true,
    },
  },
  placeholder: 'Start collaborating...',
  theme: 'snow',
});

// Create Yjs document
const ydoc = new Y.Doc();

// Get WebSocket URL (configurable via environment or defaults)
const wsUrl = import.meta.env.VITE_WS_URL || 'ws://localhost:9000';

console.log(`[Client] Connecting to ${wsUrl}`);

// Create simple provider (no awareness/cursors)
const provider = new SimpleProvider(wsUrl, ydoc);

// Define shared text type (must match server: "document")
const ytext = ydoc.getText('document');

// Manual Quill <-> Yjs binding (without awareness)
let isRemoteChange = false;

// Yjs -> Quill: Apply remote changes to editor
ytext.observe(() => {
  if (isRemoteChange) return;
  
  isRemoteChange = true;
  
  // Simple approach: rebuild entire content
  const newText = ytext.toString();
  const oldText = quill.getText();
  
  if (newText !== oldText) {
    const selection = quill.getSelection();
    quill.setText(newText);
    if (selection) {
      quill.setSelection(selection);
    }
  }
  
  isRemoteChange = false;
});

// Quill -> Yjs: Apply local changes to CRDT
quill.on('text-change', (delta: any, _oldDelta: any, source: string) => {
  if (source === 'user' && !isRemoteChange) {
    isRemoteChange = true;
    
    ydoc.transact(() => {
      let index = 0;
      delta.ops?.forEach((op: any) => {
        if (op.retain) {
          index += op.retain;
        } else if (op.insert) {
          ytext.insert(index, op.insert);
          index += op.insert.length;
        } else if (op.delete) {
          ytext.delete(index, op.delete);
        }
      });
    });
    
    isRemoteChange = false;
  }
});

// UI Elements
const statusDot = document.getElementById('status-dot')!;
const statusText = document.getElementById('status-text')!;
const usernameInput = document.getElementById('username-input') as HTMLInputElement;
const userColorDiv = document.getElementById('user-color')!;
const usersList = document.getElementById('users-list')!;

// Set user color
userColorDiv.style.background = myColor;

// Generate and set initial username
usernameInput.value = generateUsername();

// No awareness - just update UI
const updateUsername = () => {
  console.log('[Client] Username:', usernameInput.value);
};

usernameInput.addEventListener('input', updateUsername);
updateUsername();

// Connection status handlers
provider.on('status', (event: { status: string }) => {
  console.log(`[Client] WebSocket status: ${event.status}`);
  
  if (event.status === 'connected') {
    statusDot.classList.add('connected');
    statusText.textContent = 'Connected';
  } else if (event.status === 'disconnected') {
    statusDot.classList.remove('connected');
    statusText.textContent = 'Disconnected';
  } else {
    statusDot.classList.remove('connected');
    statusText.textContent = 'Connecting...';
  }
});

// Simple user list (no awareness - just show local user)
const renderUsers = () => {
  usersList.innerHTML = `
    <div class="user-item">
      <div class="user-dot" style="background: ${myColor};"></div>
      <span class="user-name">${usernameInput.value} (You)</span>
    </div>
    <div class="user-item" style="opacity: 0.5;">
      <div class="user-dot" style="background: #adb5bd;"></div>
      <span class="user-name">Connected peers (no awareness)</span>
    </div>
  `;
};

renderUsers();

// Blur editor when window loses focus
window.addEventListener('blur', () => {
  quill.blur();
});

// Log stats
console.log('[Client] Editor initialized');
console.log('[Client] Yjs document ID:', ydoc.guid);
console.log('[Client] User color:', myColor);

// Expose for debugging
(window as any).ydoc = ydoc;
(window as any).quill = quill;
(window as any).provider = provider;
(window as any).ytext = ytext;

