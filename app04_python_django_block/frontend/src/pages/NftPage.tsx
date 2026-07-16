import { useCallback, useEffect, useRef, useState } from 'react'
import { api } from '../api/client'
import { useLedgerEvents } from '../api/useWs'
import type { Nft } from '../api/types'
import { useI18n } from '../i18n'
import { Card, Field, Hash, Toast, formatTime } from '../components/ui'

/**
 * NFT studio: mint post-quantum-signed ownership certificates and browse
 * the gallery with provenance (creator fingerprint + mint transaction),
 * plus the SSI explainer tying tokens to self-sovereign identity.
 */
export function NftPage() {
  const { t, lang } = useI18n()
  const [nfts, setNfts] = useState<Nft[]>([])
  const [title, setTitle] = useState('')
  const [description, setDescription] = useState('')
  const [metadataUri, setMetadataUri] = useState('ipfs://')
  const [minting, setMinting] = useState(false)
  const [toast, setToast] = useState<string | null>(null)
  const toastTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  const showToast = useCallback((msg: string) => {
    setToast(msg)
    if (toastTimer.current) clearTimeout(toastTimer.current)
    toastTimer.current = setTimeout(() => setToast(null), 3500)
  }, [])

  const refresh = useCallback(async () => {
    try {
      setNfts(await api.nfts())
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
    if (e.type === 'BLOCK_ADDED' || e.type === 'TX_ADDED') refresh()
  })

  const mint = async (e: React.FormEvent) => {
    e.preventDefault()
    setMinting(true)
    try {
      await api.mintNft(title.trim(), description.trim(), metadataUri.trim())
      showToast(t.nft.minted)
      setTitle('')
      setDescription('')
      setMetadataUri('ipfs://')
      refresh()
    } catch (err) {
      showToast(String(err))
    } finally {
      setMinting(false)
    }
  }

  return (
    <div>
      <h1>{t.nft.title}</h1>
      <p className="page-intro">{t.nft.intro}</p>

      <Card title={t.nft.mintTitle}>
        <form onSubmit={mint}>
          <div className="form-row">
            <Field label={t.nft.name}>
              <input value={title} onChange={(e) => setTitle(e.target.value)} required />
            </Field>
            <Field label={t.nft.description}>
              <input value={description} onChange={(e) => setDescription(e.target.value)} />
            </Field>
            <Field label={t.nft.metadataUri}>
              <input value={metadataUri} onChange={(e) => setMetadataUri(e.target.value)} required />
            </Field>
            <button className="primary" type="submit" disabled={minting}>
              {minting ? t.nft.minting : t.nft.mint}
            </button>
          </div>
        </form>
      </Card>

      <h2>{t.nft.gallery}</h2>
      {nfts.length === 0 ? (
        <p className="muted">{t.nft.empty}</p>
      ) : (
        <div className="nft-grid">
          {nfts.map((n) => (
            <div className="nft-card" key={n.tokenId}>
              <div className="nft-art" aria-hidden="true">
                🎨
              </div>
              <div className="nft-body">
                <h4>{n.title}</h4>
                {n.description && <p className="muted">{n.description}</p>}
                <div>
                  {t.nft.tokenId}: <Hash value={n.tokenId} len={14} />
                </div>
                <div>
                  {t.nft.creator}: <Hash value={n.creator} len={14} />
                </div>
                <div>
                  {t.nft.tx}: <Hash value={n.txId} len={14} />
                </div>
                <div className="muted">
                  {t.nft.mintedAt}: {formatTime(n.mintedAt, lang)}
                </div>
                <div className="muted">
                  <code>{n.metadataUri}</code>
                </div>
              </div>
            </div>
          ))}
        </div>
      )}

      <Card title={t.nft.ssiTitle}>
        <p>{t.nft.ssiText}</p>
      </Card>

      <Toast message={toast} />
    </div>
  )
}
