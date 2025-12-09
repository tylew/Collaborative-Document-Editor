import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  server: {
    // port: 3000,
    host: true,
    allowedHosts: [
      'tylers-personal.flounder-blenny.ts.net', 
      'localhost', '127.0.0.1'],
  },
  // preview: {
  //   port: 3000,
  //   host: true,
  // },
});
