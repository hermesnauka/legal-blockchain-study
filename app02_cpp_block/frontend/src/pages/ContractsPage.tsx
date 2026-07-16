import { useCallback, useRef, useState } from 'react'
import { api } from '../api/client'
import type { ConsentRecord, SupplyChainEvent } from '../api/types'
import { useI18n } from '../i18n'
import { Card, Field, Toast, formatTime } from '../components/ui'

/**
 * Live smart-contract demos backed by the real contract engine:
 * medical consent management (grant/revoke + audit trail) and the
 * agricultural supply-chain trace (append-only batch timeline).
 */
export function ContractsPage() {
  const { t } = useI18n()
  const [toast, setToast] = useState<string | null>(null)
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  const showToast = useCallback((msg: string) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 3500)
  }, [])

  return (
    <div>
      <h1>{t.contracts.title}</h1>
      <p className="page-intro">{t.contracts.intro}</p>
      <MedicalPanel showToast={showToast} />
      <AgriPanel showToast={showToast} />
      <Toast message={toast} />
    </div>
  )
}

function MedicalPanel({ showToast }: { showToast: (msg: string) => void }) {
  const { t, lang } = useI18n()
  const [patientId, setPatientId] = useState('patient-001')
  const [granteeId, setGranteeId] = useState('dr-kowalski')
  const [scope, setScope] = useState('HISTORY_READ')
  const [records, setRecords] = useState<ConsentRecord[] | null>(null)

  const submit = async (granted: boolean) => {
    try {
      await api.medicalConsent(patientId.trim(), granteeId.trim(), scope, granted)
      showToast(t.contracts.submitted)
    } catch (err) {
      showToast(String(err))
    }
  }

  const load = async () => {
    try {
      setRecords(await api.medicalRecords(patientId.trim()))
    } catch (err) {
      showToast(String(err))
    }
  }

  return (
    <Card title={t.contracts.medicalTitle}>
      <p className="muted">{t.contracts.medicalIntro}</p>
      <div className="form-row">
        <Field label={t.contracts.patientId}>
          <input value={patientId} onChange={(e) => setPatientId(e.target.value)} />
        </Field>
        <Field label={t.contracts.granteeId}>
          <input value={granteeId} onChange={(e) => setGranteeId(e.target.value)} />
        </Field>
        <Field label={t.contracts.scope}>
          <select value={scope} onChange={(e) => setScope(e.target.value)}>
            {Object.entries(t.contracts.scopes).map(([k, label]) => (
              <option key={k} value={k}>
                {label}
              </option>
            ))}
          </select>
        </Field>
        <button className="primary" onClick={() => submit(true)}>
          {t.contracts.grant}
        </button>
        <button className="ghost" onClick={() => submit(false)}>
          {t.contracts.revoke}
        </button>
        <button className="ghost" onClick={load}>
          {t.contracts.loadConsents}
        </button>
      </div>
      {records !== null && (
        <>
          <h3>{t.contracts.consentRecords}</h3>
          {records.length === 0 ? (
            <p className="muted">{t.contracts.noConsents}</p>
          ) : (
            <table className="data">
              <thead>
                <tr>
                  <th>{t.contracts.granteeId}</th>
                  <th>{t.contracts.scope}</th>
                  <th />
                  <th>{t.app.timestamp}</th>
                </tr>
              </thead>
              <tbody>
                {records.map((r, i) => (
                  <tr key={i}>
                    <td>{r.granteeId}</td>
                    <td>{t.contracts.scopes[r.scope as keyof typeof t.contracts.scopes] ?? r.scope}</td>
                    <td>
                      <span className={r.granted ? 'badge ok' : 'badge bad'}>
                        {r.granted ? t.contracts.granted : t.contracts.revoked}
                      </span>
                    </td>
                    <td>{formatTime(r.timestamp, lang)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </>
      )}
    </Card>
  )
}

function AgriPanel({ showToast }: { showToast: (msg: string) => void }) {
  const { t, lang } = useI18n()
  const [batchId, setBatchId] = useState('BATCH-2026-001')
  const [stage, setStage] = useState('HARVEST')
  const [actor, setActor] = useState('')
  const [location, setLocation] = useState('')
  const [details, setDetails] = useState('')
  const [trace, setTrace] = useState<SupplyChainEvent[] | null>(null)

  const record = async () => {
    try {
      await api.agriEvent(batchId.trim(), stage, actor.trim(), location.trim(), details.trim())
      showToast(t.contracts.submitted)
      setDetails('')
    } catch (err) {
      showToast(String(err))
    }
  }

  const load = async () => {
    try {
      setTrace(await api.agriTrace(batchId.trim()))
    } catch (err) {
      showToast(String(err))
    }
  }

  return (
    <Card title={t.contracts.agriTitle}>
      <p className="muted">{t.contracts.agriIntro}</p>
      <div className="form-row">
        <Field label={t.contracts.batchId}>
          <input value={batchId} onChange={(e) => setBatchId(e.target.value)} />
        </Field>
        <Field label={t.contracts.stage}>
          <select value={stage} onChange={(e) => setStage(e.target.value)}>
            {Object.entries(t.contracts.stages).map(([k, label]) => (
              <option key={k} value={k}>
                {label}
              </option>
            ))}
          </select>
        </Field>
        <Field label={t.contracts.actor}>
          <input value={actor} onChange={(e) => setActor(e.target.value)} placeholder="Green Farm Ltd." />
        </Field>
        <Field label={t.contracts.location}>
          <input value={location} onChange={(e) => setLocation(e.target.value)} placeholder="Lublin, PL" />
        </Field>
        <Field label={t.contracts.details}>
          <input value={details} onChange={(e) => setDetails(e.target.value)} />
        </Field>
      </div>
      <div className="form-row">
        <button className="primary" onClick={record}>
          {t.contracts.recordEvent}
        </button>
        <button className="ghost" onClick={load}>
          {t.contracts.loadTrace}
        </button>
      </div>
      {trace !== null && (
        <>
          <h3>
            {t.contracts.trace}: <code>{batchId}</code>
          </h3>
          {trace.length === 0 ? (
            <p className="muted">{t.contracts.noTrace}</p>
          ) : (
            <div className="timeline">
              {trace.map((e, i) => (
                <div className="timeline-item" key={i}>
                  <div>
                    <span className="timeline-stage">
                      {t.contracts.stages[e.stage as keyof typeof t.contracts.stages] ?? e.stage}
                    </span>{' '}
                    · {formatTime(e.timestamp, lang)}
                  </div>
                  <div>
                    {e.actor} — {e.location}
                  </div>
                  {e.details && <div className="muted">{e.details}</div>}
                </div>
              ))}
            </div>
          )}
        </>
      )}
    </Card>
  )
}
