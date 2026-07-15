import { useCallback, useEffect, useRef, useState } from 'react'
import { api } from '../api/client'
import { useLedgerEvents } from '../api/useWs'
import type { Block, ChainInfo, ChainValidation, Transaction } from '../api/types'
import { useI18n } from '../i18n'
import { Card, Field, Hash, Toast, formatTime } from '../components/ui'

/**
 * Live general-ledger view: the real chain from the backend rendered as
 * linked block cards (hash → previousHash), the pending mempool, a signed
 * transfer form, mining, validation and derived balances.
 */
export function LedgerPage() {
  const { t } = useI18n()
  const [chain, setChain] = useState<ChainInfo | null>(null)
  const [pending, setPending] = useState<Transaction[]>([])
  const [balances, setBalances] = useState<Record<string, number>>({})
  const [validation, setValidation] = useState<ChainValidation | null>(null)
  const [selected, setSelected] = useState<number | null>(null)
  const [newestIndex, setNewestIndex] = useState<number | null>(null)
  const [mining, setMining] = useState(false)
  const [toast, setToast] = useState<string | null>(null)
  const [recipient, setRecipient] = useState('')
  const [amount, setAmount] = useState('1')
  const [memo, setMemo] = useState('')
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  const showToast = useCallback((msg: string) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 3500)
  }, [])

  const refresh = useCallback(async () => {
    try {
      const [c, p, b] = await Promise.all([api.chain(), api.pending(), api.balances()])
      setChain(c)
      setPending(p)
      setBalances(b)
    } catch {
      // offline banner handled globally by the API client
    }
  }, [])

  useEffect(() => {
    refresh()
    return () => {
      if (toastTimer.current) clearTimeout(toastTimer.current)
    }
  }, [refresh])

  useLedgerEvents((e) => {
    if (e.type === 'BLOCK_ADDED') {
      const block = e.data as Block
      setNewestIndex(block.index)
      refresh()
    } else if (e.type === 'TX_ADDED' || e.type === 'CHAIN_REPLACED') {
      refresh()
    }
  })

  const mine = async () => {
    setMining(true)
    try {
      await api.mine()
      showToast(t.ledger.minedOk)
    } catch (err) {
      showToast(String(err))
    } finally {
      setMining(false)
    }
  }

  const validate = async () => {
    try {
      setValidation(await api.validate())
    } catch (err) {
      showToast(String(err))
    }
  }

  const submitTx = async (e: React.FormEvent) => {
    e.preventDefault()
    try {
      await api.addTransaction(recipient.trim(), Number(amount), memo.trim() || undefined)
      showToast(t.ledger.txAdded)
      setRecipient('')
      setMemo('')
      refresh()
    } catch (err) {
      showToast(String(err))
    }
  }

  const selectedBlock = chain?.blocks.find((b) => b.index === selected) ?? null

  return (
    <div>
      <h1>{t.ledger.title}</h1>
      <p className="page-intro">{t.ledger.intro}</p>

      <div className="form-row">
        <button className="primary" onClick={mine} disabled={mining}>
          {mining ? t.ledger.mining : t.ledger.mine}
        </button>
        <button className="ghost" onClick={validate}>
          {t.ledger.validate}
        </button>
        {validation && (
          <span className={validation.valid ? 'badge ok' : 'badge bad'}>
            {validation.valid ? t.ledger.chainValid : t.ledger.chainInvalid}
            {!validation.valid && ` — ${validation.message}`}
          </span>
        )}
      </div>

      {chain && (
        <div className="chain-scroll" aria-label="Blockchain">
          {chain.blocks.map((b, i) => (
            <BlockCard
              key={b.hash}
              block={b}
              isNew={b.index === newestIndex}
              isSelected={b.index === selected}
              isLast={i === chain.blocks.length - 1}
              onSelect={() => setSelected(selected === b.index ? null : b.index)}
            />
          ))}
        </div>
      )}
      <p className="muted">{t.ledger.clickToExpand}</p>

      {selectedBlock && <BlockDetail block={selectedBlock} />}

      <Card title={t.ledger.pendingPool}>
        {pending.length === 0 ? (
          <p className="muted">{t.ledger.noPending}</p>
        ) : (
          pending.map((tx) => <TxRow key={tx.id} tx={tx} />)
        )}
      </Card>

      <Card title={t.ledger.addTransaction}>
        <form onSubmit={submitTx}>
          <div className="form-row">
            <Field label={t.ledger.recipient}>
              <input value={recipient} onChange={(e) => setRecipient(e.target.value)} required placeholder="alice" />
            </Field>
            <Field label={t.ledger.amount}>
              <input type="number" min="0.000001" step="any" value={amount} onChange={(e) => setAmount(e.target.value)} required />
            </Field>
            <Field label={t.ledger.memo}>
              <input value={memo} onChange={(e) => setMemo(e.target.value)} />
            </Field>
            <button className="primary" type="submit">
              {t.app.send}
            </button>
          </div>
        </form>
      </Card>

      <Card title={t.ledger.balances}>
        {Object.keys(balances).length === 0 ? (
          <p className="muted">{t.ledger.noBalances}</p>
        ) : (
          <table className="data">
            <thead>
              <tr>
                <th>{t.ledger.address}</th>
                <th>{t.ledger.balance}</th>
              </tr>
            </thead>
            <tbody>
              {Object.entries(balances).map(([addr, bal]) => (
                <tr key={addr}>
                  <td>
                    <Hash value={addr} len={24} />
                  </td>
                  <td>{bal} LGC</td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </Card>

      <Toast message={toast} />
    </div>
  )
}

function BlockCard({
  block,
  isNew,
  isSelected,
  isLast,
  onSelect,
}: {
  block: Block
  isNew: boolean
  isSelected: boolean
  isLast: boolean
  onSelect: () => void
}) {
  const { t, lang } = useI18n()
  const cls = ['block-card', isNew ? 'new' : '', isSelected ? 'selected' : ''].filter(Boolean).join(' ')
  return (
    <>
      <div className={cls} onClick={onSelect} role="button" tabIndex={0} onKeyDown={(e) => e.key === 'Enter' && onSelect()}>
        <div className="block-index">
          {block.index === 0 ? `⛏ ${t.ledger.genesis}` : `${t.ledger.block} #${block.index}`}
        </div>
        <div className="block-meta">
          <span>
            {t.ledger.hash}: <Hash value={block.hash} len={12} />
          </span>
          <span>
            {t.ledger.prevHash}: <Hash value={block.previousHash} len={12} />
          </span>
          <span>
            {block.transactions.length} {t.ledger.txCount} · {block.consensusType}
          </span>
          <span>{formatTime(block.timestamp, lang)}</span>
        </div>
      </div>
      {!isLast && (
        <span className="chain-arrow" aria-hidden="true">
          →
        </span>
      )}
    </>
  )
}

function BlockDetail({ block }: { block: Block }) {
  const { t, lang } = useI18n()
  return (
    <Card title={`${t.ledger.block} #${block.index}`}>
      <dl className="facts">
        <dt>{t.ledger.hash}</dt>
        <dd>
          <Hash value={block.hash} len={64} />
        </dd>
        <dt>{t.ledger.prevHash}</dt>
        <dd>
          <Hash value={block.previousHash} len={64} />
        </dd>
        <dt>{t.ledger.merkleRoot}</dt>
        <dd>
          <Hash value={block.merkleRoot} len={64} />
        </dd>
        <dt>{t.ledger.validator}</dt>
        <dd>
          <Hash value={block.validatorId} len={24} />
        </dd>
        <dt>{t.ledger.consensusType}</dt>
        <dd>{block.consensusType}</dd>
        <dt>{t.ledger.nonce}</dt>
        <dd>{block.nonce}</dd>
        <dt>{t.ledger.proof}</dt>
        <dd>
          <Hash value={block.proof} len={48} />
        </dd>
        <dt>{t.app.timestamp}</dt>
        <dd>{formatTime(block.timestamp, lang)}</dd>
      </dl>
      <h3>{t.ledger.transactions}</h3>
      {block.transactions.map((tx) => (
        <TxRow key={tx.id} tx={tx} />
      ))}
    </Card>
  )
}

function TxRow({ tx }: { tx: Transaction }) {
  const { t } = useI18n()
  return (
    <div className="tx-row">
      <div>
        <span className="tx-type">{tx.type}</span> · {t.ledger.txId} <Hash value={tx.id} len={12} />
      </div>
      <div>
        {t.ledger.sender}: <Hash value={tx.sender} len={16} /> → {t.ledger.recipient}:{' '}
        <Hash value={tx.recipient} len={16} /> · {tx.amount} LGC
      </div>
      {tx.signature && (
        <div>
          {t.ledger.signature}: <Hash value={tx.signature} len={24} />
        </div>
      )}
    </div>
  )
}
