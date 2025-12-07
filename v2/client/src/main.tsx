import ReactDOM from 'react-dom/client';
import App from './App.tsx';
import './index.css';

// NOTE: StrictMode removed to prevent double-mounting issues with Quill editor
// StrictMode causes useEffect to run twice, which creates multiple Quill instances
ReactDOM.createRoot(document.getElementById('root')!).render(<App />);
