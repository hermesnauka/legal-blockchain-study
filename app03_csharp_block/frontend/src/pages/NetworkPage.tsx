import { useCallback, useEffect, useRef, useState } from 'react'
import { api } from '../api/client'
import { useLedgerEvents } from '../api/useWs'
import type { NodeInfo, Wallet } from '../api/types'
import { useI18n } from '../i18n'
import { Card, Field, Hash, Toast } from '../components/ui'

/**
 * P2P view: this node's identity, peer connect/sync controls, an animated
 * two-node diagram and the explanation of the post-quantum handshake that
 * secures the channel between two remote nodes.
 */
export function NetworkPage() {
  const { t } = useI18n()
  const [node, setNode] = useState<NodeInfo | null>(null)
  const [wallet, setWallet] = useState<Wallet | null>(null)
  const [peerUrl, setPeerUrl] = useState('ws://localhost:8101/ws/p2p')
  const [connecting, setConnecting] = useState(false)
  const [syncing, setSyncing] = useState(false)
  const [syncResult, setSyncResult] = useState<string | null>(null)
  const [toast, setToast] = useState<string | null>(null)
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  const showToast = useCallback((msg: string) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 3500)
  }, [])

  const refresh = useCallback(async () => {
    try {
      const [n, w] = await Promise.all([api.node(), api.wallet()])
      setNode(n)
      setWallet(w)
    } catch {
      // offline banner handled globally
    }
  }, [])

  useEffect(() => {
    refresh()
    return () => {
      if (toastTimer.current) clearTimeout(toastTimer.current)
    }
  }, [refresh])

  useLedgerEvents((e) => {
    if (e.type === 'PEER_CONNECTED' || e.type === 'CHAIN_REPLACED' || e.type === 'BLOCK_ADDED') refresh()
  })

  const connect = async () => {
    setConnecting(true)
    try {
      const res = await api.p2pConnect(peerUrl.trim())
      showToast(`${t.network.connected}: ${res.peer ?? peerUrl}`)
      // The PQC handshake completes asynchronously right after the WS opens;
      // refresh once more after a beat so the peer list reflects it.
      refresh()
      setTimeout(refresh, 1500)
    } catch (err) {
      showToast(String(err))
    } finally {
      setConnecting(false)
    }
  }

  const sync = async () => {
    setSyncing(true)
    try {
      const res = await api.p2pSync()
      setSyncResult(`${res.result} (${t.network.chainLength}: ${res.chainLength})`)
      refresh()
    } catch (err) {
      showToast(String(err))
    } finally {
      setSyncing(false)
    }
  }

  const hasPeer = (node?.peers.length ?? 0) > 0

  return (
    <div>
      <h1>{t.network.title}</h1>
      <p className="page-intro">{t.network.intro}</p>

      <Card title={t.network.thisNode}>
        <dl className="facts">
          <dt>{t.network.nodeId}</dt>
          <dd>
            <Hash value={node?.nodeId ?? ''} len={32} />
          </dd>
          <dt>{t.network.port}</dt>
          <dd>{node?.port ?? '—'}</dd>
          <dt>{t.network.walletFp}</dt>
          <dd>
            <Hash value={wallet?.fingerprint ?? ''} len={32} />
          </dd>
          <dt>{t.network.algorithm}</dt>
          <dd>{wallet?.algorithm ?? '—'}</dd>
          <dt>{t.network.chainLength}</dt>
          <dd>{node?.chainLength ?? '—'}</dd>
          <dt>{t.network.activeConsensus}</dt>
          <dd>{node?.consensus ?? '—'}</dd>
        </dl>
      </Card>

      <Card title={t.network.diagramTitle}>
        <div className="net-diagram">
          <div className="net-node">
            <span className="icon">🖥</span>
            <span>{t.network.nodeA}</span>
            <Hash value={node?.nodeId ?? ''} len={10} />
          </div>
          <div className="net-link">
            {hasPeer && (
              <>
                <span className="net-packet" />
                <span className="net-packet reverse" />
              </>
            )}
            <span className="net-link-label">ML-KEM → AES-256-GCM</span>
          </div>
          <div className="net-node">
            <span className="icon">{hasPeer ? '🖥' : '❓'}</span>
            <span>{t.network.nodeB}</span>
            {hasPeer ? <Hash value={node!.peers[0].nodeId} len={10} /> : <span className="muted">—</span>}
          </div>
        </div>
      </Card>

      <Card title={t.network.connectTitle}>
        <div className="form-row">
          <Field label={t.network.peerUrl}>
            <input value={peerUrl} onChange={(e) => setPeerUrl(e.target.value)} />
          </Field>
          <button className="primary" onClick={connect} disabled={connecting}>
            {connecting ? t.network.connecting : t.network.connect}
          </button>
          <button className="ghost" onClick={sync} disabled={syncing || !hasPeer}>
            {syncing ? t.network.syncing : t.network.sync}
          </button>
        </div>
        {syncResult && (
          <p>
            {t.network.syncResult}: <span className="badge neutral">{syncResult}</span>
          </p>
        )}
        <h3>{t.network.peers}</h3>
        {!hasPeer ? (
          <p className="muted">{t.network.noPeers}</p>
        ) : (
          <table className="data">
            <tbody>
              {node!.peers.map((p) => (
                <tr key={p.nodeId}>
                  <td>
                    <Hash value={p.nodeId} len={32} />
                  </td>
                  <td>
                    <code>{p.url}</code>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </Card>

      <Card title={t.network.handshakeTitle}>
        <ol className="steps">
          {t.network.handshakeSteps.map((step, i) => (
            <li key={i}>{step}</li>
          ))}
        </ol>
        <p className="muted">{t.network.whyMitmSafe}</p>
      </Card>

      <Toast message={toast} />
    </div>
  )
}
