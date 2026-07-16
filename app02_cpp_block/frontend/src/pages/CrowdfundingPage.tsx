import { useCallback, useEffect, useRef, useState } from 'react'
import { api } from '../api/client'
import { useLedgerEvents } from '../api/useWs'
import { useI18n } from '../i18n'
import { Card, Field, Hash, Toast, formatTime } from '../components/ui'

/** Memo convention that marks a transfer as a campaign contribution. */
const CAMPAIGN_PREFIX = 'campaign:'

interface CampaignStats {
  name: string
  total: number
  contributions: number
  contributors: Set<string>
  latest: number
}

/**
 * Crowdfunding dashboard (FE-08): contributions are ordinary signed
 * transfers whose memo is "campaign:<name>". All aggregates are computed
 * client-side from confirmed chain data — nothing here trusts a database;
 * anyone can recompute the same totals from their own copy of the ledger.
 */
export function CrowdfundingPage() {
  const { t, lang } = useI18n()
  const [campaigns, setCampaigns] = useState<CampaignStats[]>([])
  const [campaign, setCampaign] = useState('save-the-bees')
  const [beneficiary, setBeneficiary] = useState('')
  const [amount, setAmount] = useState('5')
  const [toast, setToast] = useState<string | null>(null)
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  const showToast = useCallback((msg: string) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 3500)
  }, [])

  const refresh = useCallback(async () => {
    try {
      const chain = await api.chain()
      const byName = new Map<string, CampaignStats>()
      for (const block of chain.blocks) {
        for (const tx of block.transactions) {
          const memo = tx.payload?.memo
          if (tx.type !== 'TRANSFER' || !memo || !memo.toLowerCase().startsWith(CAMPAIGN_PREFIX)) continue
          const name = memo.slice(CAMPAIGN_PREFIX.length).trim()
          if (!name) continue
          const stats =
            byName.get(name) ?? { name, total: 0, contributions: 0, contributors: new Set<string>(), latest: 0 }
          stats.total += Number(tx.amount)
          stats.contributions += 1
          stats.contributors.add(tx.sender)
          stats.latest = Math.max(stats.latest, tx.timestamp)
          byName.set(name, stats)
        }
      }
      setCampaigns([...byName.values()].sort((a, b) => b.total - a.total))
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
    if (e.type === 'BLOCK_ADDED' || e.type === 'CHAIN_REPLACED') refresh()
  })

  const contribute = async (e: React.FormEvent) => {
    e.preventDefault()
    try {
      await api.addTransaction(beneficiary.trim(), Number(amount), `${CAMPAIGN_PREFIX}${campaign.trim()}`)
      showToast(t.crowdfunding.contributed)
    } catch (err) {
      showToast(String(err))
    }
  }

  return (
    <div>
      <h1>{t.crowdfunding.title}</h1>
      <p className="page-intro">{t.crowdfunding.intro}</p>

      <Card title={t.crowdfunding.contribute}>
        <form onSubmit={contribute}>
          <div className="form-row">
            <Field label={t.crowdfunding.campaign}>
              <input value={campaign} onChange={(e) => setCampaign(e.target.value)} required />
            </Field>
            <Field label={t.crowdfunding.beneficiary}>
              <input value={beneficiary} onChange={(e) => setBeneficiary(e.target.value)} required placeholder="ngo-treasury" />
            </Field>
            <Field label={t.crowdfunding.amount}>
              <input type="number" min="0.000001" step="any" value={amount} onChange={(e) => setAmount(e.target.value)} required />
            </Field>
            <button className="primary" type="submit">
              {t.crowdfunding.send}
            </button>
          </div>
        </form>
      </Card>

      <h2>{t.crowdfunding.dashboard}</h2>
      {campaigns.length === 0 ? (
        <p className="muted">{t.crowdfunding.noCampaigns}</p>
      ) : (
        campaigns.map((c) => (
          <Card key={c.name} title={c.name}>
            <div className="stat-row">
              <div className="stat">
                <div className="k">{t.crowdfunding.total}</div>
                <div className="v">{c.total} LGC</div>
              </div>
              <div className="stat">
                <div className="k">{t.crowdfunding.contributions}</div>
                <div className="v">{c.contributions}</div>
              </div>
              <div className="stat">
                <div className="k">{t.crowdfunding.contributors}</div>
                <div className="v">{c.contributors.size}</div>
              </div>
              <div className="stat">
                <div className="k">{t.crowdfunding.latest}</div>
                <div className="v">{formatTime(c.latest, lang)}</div>
              </div>
            </div>
            <p className="muted">
              {[...c.contributors].map((addr) => (
                <span key={addr} style={{ marginRight: '0.8rem' }}>
                  <Hash value={addr} len={16} />
                </span>
              ))}
            </p>
          </Card>
        ))
      )}

      <Toast message={toast} />
    </div>
  )
}
