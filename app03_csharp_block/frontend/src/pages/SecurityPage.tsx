import { useEffect, useState } from 'react'
import { api } from '../api/client'
import type { Wallet } from '../api/types'
import { useI18n } from '../i18n'
import { Card, Hash } from '../components/ui'

/**
 * PQC education view: Bitcoin-vs-this-app comparison, Shor / harvest-now
 * threat explanations, NIST standardization context, a handshake diagram
 * and a gallery of attacker scenarios with their defenses. Key sizes shown
 * are real values fetched from the running node's wallet.
 */
export function SecurityPage() {
  const { t } = useI18n()
  const [wallet, setWallet] = useState<Wallet | null>(null)

  useEffect(() => {
    api.wallet().then(setWallet).catch(() => {})
  }, [])

  const rows = Object.values(t.securityPage.rows)

  return (
    <div>
      <h1>{t.securityPage.title}</h1>
      <p className="page-intro">{t.securityPage.intro}</p>

      <Card title={t.securityPage.comparisonTitle}>
        <table className="data compare-table">
          <thead>
            <tr>
              <th />
              <th>{t.securityPage.bitcoinCol}</th>
              <th>{t.securityPage.thisAppCol}</th>
            </tr>
          </thead>
          <tbody>
            {rows.map(([label, bitcoin, thisApp]) => (
              <tr key={label}>
                <th>{label}</th>
                <td>{bitcoin}</td>
                <td>{thisApp}</td>
              </tr>
            ))}
          </tbody>
        </table>
        {wallet && (
          <div className="stat-row" style={{ marginTop: '0.8rem' }}>
            <div className="stat">
              <div className="k">{t.network.algorithm}</div>
              <div className="v">{wallet.algorithm}</div>
            </div>
            <div className="stat">
              <div className="k">{t.network.walletFp}</div>
              <div className="v">
                <Hash value={wallet.fingerprint} len={20} />
              </div>
            </div>
            <div className="stat">
              <div className="k">Public key</div>
              <div className="v">{Math.round((wallet.publicKey.length * 3) / 4 / 1024 * 10) / 10} KiB</div>
            </div>
          </div>
        )}
      </Card>

      <Card title={t.securityPage.shorTitle}>
        <p>{t.securityPage.shorText}</p>
      </Card>

      <Card title={t.securityPage.harvestTitle}>
        <p>{t.securityPage.harvestText}</p>
      </Card>

      <Card title={t.securityPage.nistTitle}>
        <p>{t.securityPage.nistText}</p>
      </Card>

      <Card title={t.securityPage.qkdTitle}>
        <p>{t.securityPage.qkdText}</p>
      </Card>

      <Card title={t.securityPage.handshakeDiagramTitle}>
        <HandshakeDiagram
          hello={t.securityPage.handshakeLabels.hello}
          encaps={t.securityPage.handshakeLabels.encaps}
          channel={t.securityPage.handshakeLabels.channel}
          nodeA={t.network.nodeA}
          nodeB={t.network.nodeB}
        />
      </Card>

      <h2>{t.securityPage.attacksTitle}</h2>
      <div className="attack-grid">
        {t.securityPage.attacks.map((a) => (
          <div className="attack-card" key={a.title}>
            <h4>{a.title}</h4>
            <p>
              <span className="attack-label atk">⚔ </span>
              {a.attack}
            </p>
            <p>
              <span className="attack-label def">🛡 </span>
              {a.defense}
            </p>
          </div>
        ))}
      </div>
    </div>
  )
}

/** Sequence-style SVG of the three-step post-quantum handshake. */
function HandshakeDiagram({
  hello,
  encaps,
  channel,
  nodeA,
  nodeB,
}: {
  hello: string
  encaps: string
  channel: string
  nodeA: string
  nodeB: string
}) {
  const teal = 'var(--accent)'
  const dim = 'var(--text-dim)'
  return (
    <svg className="handshake-svg" viewBox="0 0 720 260" role="img" aria-label={`${hello}; ${encaps}; ${channel}`}>
      <defs>
        <marker id="arr" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
          <path d="M 0 0 L 10 5 L 0 10 z" fill={teal} />
        </marker>
      </defs>
      {/* lifelines */}
      <text x="90" y="24" textAnchor="middle" fill="currentColor" fontSize="14" fontWeight="700">
        {nodeA}
      </text>
      <text x="630" y="24" textAnchor="middle" fill="currentColor" fontSize="14" fontWeight="700">
        {nodeB}
      </text>
      <line x1="90" y1="36" x2="90" y2="250" stroke={dim} strokeDasharray="4 4" />
      <line x1="630" y1="36" x2="630" y2="250" stroke={dim} strokeDasharray="4 4" />
      {/* 1. signed hello A -> B */}
      <line x1="90" y1="70" x2="626" y2="70" stroke={teal} strokeWidth="2" markerEnd="url(#arr)" />
      <text x="360" y="60" textAnchor="middle" fill="currentColor" fontSize="12">
        {hello}
      </text>
      {/* 2. encapsulation B -> A */}
      <line x1="630" y1="130" x2="94" y2="130" stroke={teal} strokeWidth="2" markerEnd="url(#arr)" />
      <text x="360" y="120" textAnchor="middle" fill="currentColor" fontSize="12">
        {encaps}
      </text>
      {/* 3. encrypted channel, both directions */}
      <line x1="90" y1="200" x2="626" y2="200" stroke={teal} strokeWidth="4" markerEnd="url(#arr)" opacity="0.85" />
      <line x1="630" y1="222" x2="94" y2="222" stroke={teal} strokeWidth="4" markerEnd="url(#arr)" opacity="0.85" />
      <text x="360" y="190" textAnchor="middle" fill="currentColor" fontSize="12">
        {channel}
      </text>
    </svg>
  )
}
