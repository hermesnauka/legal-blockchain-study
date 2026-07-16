import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Dev proxy: the C#/ASP.NET Core backend (app03_csharp_block) listens on :8100.
// /api  -> REST endpoints
// /ws   -> WebSocket endpoints (live ledger events, P2P)
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5175,
    proxy: {
      '/api': {
        target: 'http://localhost:8100',
        changeOrigin: true,
      },
      '/ws': {
        target: 'http://localhost:8100',
        ws: true,
      },
    },
  },
})
