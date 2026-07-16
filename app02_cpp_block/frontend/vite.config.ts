import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Dev proxy: the C++/Drogon backend (app02_cpp_block) listens on :8090.
// /api  -> REST endpoints
// /ws   -> WebSocket endpoints (live ledger events, P2P)
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5174,
    proxy: {
      '/api': {
        target: 'http://localhost:8090',
        changeOrigin: true,
      },
      '/ws': {
        target: 'http://localhost:8090',
        ws: true,
      },
    },
  },
})
