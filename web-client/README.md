# Web Client

React-based collaborative text editor using Yjs CRDTs and WebSocket.

## Features

- ✅ **Real-time Sync**: Instant updates across all connected clients
- ✅ **React 18**: Modern component architecture with hooks
- ✅ **Tailwind CSS**: Utility-first styling with responsive design
- ✅ **Rich Text**: Quill editor with formatting toolbar
- ✅ **CRDT-based**: Conflict-free editing with Yjs
- ✅ **Type-safe**: Full TypeScript support
- ✅ **Custom Provider**: Direct raw Yjs updates (no complex protocol)

## Setup

```bash
# Install dependencies
npm install

# Start dev server
npm run dev

# Open http://localhost:3000
```

## Architecture

### Tech Stack
- **React 18** - UI framework
- **TypeScript** - Type safety
- **Tailwind CSS** - Styling
- **Yjs** - CRDT library
- **React-Quill** - Rich text editor component
- **Vite** - Build tool & dev server

### Structure

```
web-client/
├── src/
│   ├── App.tsx            # Main React component
│   ├── main.tsx           # React entry point
│   ├── simple-provider.ts # Custom WebSocket provider
│   ├── index.css          # Tailwind imports
│   └── vite-env.d.ts      # TypeScript declarations
├── index.html
├── package.json
├── tailwind.config.js     # Tailwind configuration
├── postcss.config.js      # CSS processing
├── tsconfig.json          # TypeScript config
└── vite.config.ts         # Vite config
```

### Data Flow

```
User types in Quill
       ↓
React state update
       ↓
Yjs YText.insert/delete
       ↓
SimpleProvider sends raw update
       ↓
WebSocket → Server
       ↓
Server broadcasts to others
       ↓
Other clients receive update
       ↓
Yjs applies update
       ↓
React re-renders editor
```

## Configuration

### Environment Variables

Create `.env` file:

```bash
VITE_WS_URL=ws://localhost:9000
```

The `VITE_` prefix is required for Vite to expose vars to client code.

### Tailwind

Customize `tailwind.config.js`:

```javascript
theme: {
  extend: {
    colors: {
      primary: '#your-color',
    },
  },
}
```

### Quill Toolbar

Edit `App.tsx` modules config:

```typescript
const modules = {
  toolbar: [
    ['bold', 'italic', 'underline'],
    // Add more options
  ],
};
```

## Components

### SimpleProvider

Custom WebSocket provider that sends raw Yjs updates:

```typescript
const provider = new SimpleProvider(wsUrl, ydoc);

provider.on('status', (event) => {
  // 'connecting' | 'connected' | 'disconnected'
});
```

**No awareness/cursor sync** - keeps protocol simple and focused on document content.

### App.tsx

Main React component with:
- `useState` for username, status, editor value
- `useRef` for Yjs doc, provider, Quill instance  
- `useEffect` for initialization & cleanup
- Manual Quill ↔ Yjs synchronization

## Development

```bash
# Start dev server with hot reload
npm run dev

# Type checking
npm run build

# Preview production build
npm run preview
```

## Production Build

```bash
npm run build
# Output in ./dist

# Serve with any static server
npx serve dist
```

## Browser Support

- Chrome/Edge 90+
- Firefox 88+
- Safari 14+

Requires: WebSocket, ES2020

## Troubleshooting

**Connection failed:**
- Check server is running on port 9000
- Verify WebSocket URL in console
- Check browser console for errors

**Styles not applying:**
```bash
# Rebuild Tailwind
npm run dev
```

**TypeScript errors:**
```bash
# Reinstall types
npm install --save-dev @types/react @types/react-dom
```

**Module not found:**
```bash
# Clean install
rm -rf node_modules package-lock.json
npm install
```

## Testing Multi-Client

1. Start server: `cd ../server && make run`
2. Start client: `npm run dev`
3. Open `localhost:3000` in multiple browser tabs
4. Type in one tab → see updates in others instantly

## Customization

### Colors

Edit `App.tsx`:

```typescript
const userColors = [
  '#30bced', // blue
  '#6eeb83', // green
  // add your colors
];
```

### Username Generator

Edit `generateUsername()` function in `App.tsx`:

```typescript
const adjectives = ['Your', 'Custom', 'Words'];
const nouns = ['Animal', 'Names'];
```

### Layout

All styling uses Tailwind utilities. Edit JSX classes:

```tsx
<div className="grid grid-cols-1 lg:grid-cols-4 gap-6">
  {/* Responsive grid */}
</div>
```

## Dependencies

### Runtime
- `react`, `react-dom` - UI framework
- `react-quill` - Quill wrapper
- `yjs` - CRDT library
- `quill` - Editor

### Development
- `@vitejs/plugin-react` - Vite React support
- `tailwindcss` - CSS framework
- `typescript` - Type safety
- `@types/*` - Type definitions

## Performance

- Code splitting via Vite
- Lazy loading for large docs
- Efficient Yjs binary protocol
- React optimized re-renders

## Limitations

- No cursor/awareness (server doesn't support it)
- Text-only sync (no rich formatting sync)
- Single document per connection
- No offline support

## Next Steps

See parent [README.md](../README.md) for full project overview.
