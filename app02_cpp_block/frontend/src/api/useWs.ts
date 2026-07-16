import { useEffect, useRef } from 'react'
import { API_BASE } from './client'
import type { LedgerEvent } from './types'

/**
 * Subscribes to the backend live-event WebSocket (/ws/events, proxied by
 * Vite to the C++/Drogon node). Automatically reconnects with a 2 s
 * back-off when the connection drops, so the UI recovers by itself when
 * the backend restarts.
 */
export function useLedgerEvents(onEvent: (e: LedgerEvent) => void) {
  const handler = useRef(onEvent)
  handler.current = onEvent

  useEffect(() => {
    let ws: WebSocket | null = null
    let timer: ReturnType<typeof setTimeout> | null = null
    let closed = false

    const connect = () => {
      const proto = window.location.protocol === 'https:' ? 'wss' : 'ws'
      const url = API_BASE
        ? `${API_BASE.replace(/^http/i, 'ws')}/ws/events`
        : `${proto}://${window.location.host}/ws/events`
      ws = new WebSocket(url)
      ws.onmessage = (msg) => {
        try {
          handler.current(JSON.parse(msg.data) as LedgerEvent)
        } catch {
          // ignore malformed frames
        }
      }
      ws.onclose = () => {
        if (!closed) timer = setTimeout(connect, 2000)
      }
      ws.onerror = () => ws?.close()
    }

    connect()
    return () => {
      closed = true
      if (timer) clearTimeout(timer)
      ws?.close()
    }
  }, [])
}
