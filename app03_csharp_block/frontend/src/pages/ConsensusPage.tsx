import { useCallback, useEffect, useRef, useState } from 'react'
import { api } from '../api/client'
import { useLedgerEvents } from '../api/useWs'
import type { ConsensusState } from '../api/types'
import { useI18n } from '../i18n'
import { Accordion, Card, Toast } from '../components/ui'

/** Keys of the encyclopedia entries that can be activated live on the backend. */
const LIVE_STRATEGIES: Record<string, string> = {
  pow: 'POW',
  pos: 'POS',
  bft: 'BFT',
}

/**
 * Consensus encyclopedia: expandable, bilingual explanations of nine
 * consensus families, plus a "try it live" switch that hot-swaps the
 * strategy actually used by the backend for the next mined block.
 */
export function ConsensusPage() {
  const { t } = useI18n()
  const [state, setState] = useState<ConsensusState | null>(null)
  const [toast, setToast] = useState<string | null>(null)
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  const refresh = useCallback(async () => {
    try {
      setState(await api.consensus())
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
    if (e.type === 'CONSENSUS_CHANGED') refresh()
  })

  const activate = async (strategy: string) => {
    try {
      await api.setConsensus(strategy)
      setToast(t.consensusPage.switched)
      if (toastTimer.current) clearTimeout(toastTimer.current)
      toastTimer.current = setTimeout(() => setToast(null), 3000)
      refresh()
    } catch (err) {
      setToast(String(err))
    }
  }

  const entries = Object.entries(t.consensusPage.entries)

  return (
    <div>
      <h1>{t.consensusPage.title}</h1>
      <p className="page-intro">{t.consensusPage.intro}</p>

      <Card title={t.consensusPage.activeTitle}>
        <p>
          {t.consensusPage.activeLabel}: <span className="badge neutral">{state?.active ?? '…'}</span>
        </p>
      </Card>

      {entries.map(([key, entry]) => {
        const live = LIVE_STRATEGIES[key]
        const isActive = live !== undefined && state?.active === live
        return (
          <Accordion
            key={key}
            header={
              <span>
                {entry.name}
                <span className="tagline">{entry.tagline}</span>
                {isActive && <span className="badge ok" style={{ marginLeft: '0.6rem' }}>●</span>}
              </span>
            }
          >
            <h3>{t.consensusPage.howItWorks}</h3>
            <p>{entry.how}</p>
            <ol className="steps">
              {entry.steps.map((s, i) => (
                <li key={i}>{s}</li>
              ))}
            </ol>
            <dl className="facts">
              <dt>{t.consensusPage.energy}</dt>
              <dd>{entry.energy}</dd>
              <dt>{t.consensusPage.security}</dt>
              <dd>{entry.security}</dd>
              <dt>{t.consensusPage.networks}</dt>
              <dd>{entry.networks}</dd>
            </dl>
            <div className="pros-cons">
              <div className="pros">
                <h4>{t.consensusPage.pros}</h4>
                <ul>
                  {entry.pros.map((p, i) => (
                    <li key={i}>{p}</li>
                  ))}
                </ul>
              </div>
              <div className="cons">
                <h4>{t.consensusPage.cons}</h4>
                <ul>
                  {entry.cons.map((c, i) => (
                    <li key={i}>{c}</li>
                  ))}
                </ul>
              </div>
            </div>
            {live && !isActive && (
              <button className="ghost" onClick={() => activate(live)}>
                {t.consensusPage.switchTo} {live}
              </button>
            )}
          </Accordion>
        )
      })}

      <Toast message={toast} />
    </div>
  )
}
