import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Dev proxy: the Spring Boot backend listens on :8080.
// /api  -> REST endpoints
// /ws   -> WebSocket endpoints (live ledger events, P2P)
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/ws': {
        target: 'http://localhost:8080',
        ws: true,
      },
    },
  },
})
