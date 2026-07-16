import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Dev proxy: the Python/Django backend (app04_python_django_block) listens on :8110.
// /api  -> REST endpoints
// /ws   -> WebSocket endpoints (live ledger events, P2P)
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5176,
    proxy: {
      '/api': {
        target: 'http://localhost:8110',
        changeOrigin: true,
      },
      '/ws': {
        target: 'http://localhost:8110',
        ws: true,
      },
    },
  },
})
