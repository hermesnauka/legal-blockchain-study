import type {
  ChainInfo,
  ChainValidation,
  Block,
  Transaction,
  NodeInfo,
  Wallet,
  ConsensusState,
  Nft,
  ContractResult,
  ConsentRecord,
  SupplyChainEvent,
  ConnectResult,
  SyncResult,
} from './types'

/**
 * Tiny fetch wrapper. Network failures (backend down) are broadcast as a
 * window event so the app shell can show a global "backend offline" banner
 * without every page having to manage that state.
 */
export const API_STATUS_EVENT = 'api-status'

/**
 * Optional absolute backend base URL (FE-01). Leave unset in development —
 * the Vite dev server proxies /api and /ws to :8090. Set VITE_API_URL
 * (e.g. http://node.example:8090) to point a built SPA at a remote node.
 */
export const API_BASE = ((import.meta.env.VITE_API_URL as string | undefined) ?? '').replace(/\/+$/, '')

function broadcast(online: boolean) {
  window.dispatchEvent(new CustomEvent(API_STATUS_EVENT, { detail: online }))
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  let res: Response
  try {
    res = await fetch(API_BASE + path, {
      headers: { 'Content-Type': 'application/json' },
      ...init,
    })
  } catch (e) {
    broadcast(false)
    throw e
  }
  broadcast(true)
  if (!res.ok) {
    const body = await res.text().catch(() => '')
    throw new Error(`HTTP ${res.status}: ${body || res.statusText}`)
  }
  if (res.status === 204) return undefined as T
  return (await res.json()) as T
}

const get = <T>(path: string) => request<T>(path)
const post = <T>(path: string, body?: unknown) =>
  request<T>(path, { method: 'POST', body: body === undefined ? undefined : JSON.stringify(body) })

export const api = {
  node: () => get<NodeInfo>('/api/node'),
  chain: () => get<ChainInfo>('/api/chain'),
  validate: () => get<ChainValidation>('/api/chain/validate'),
  mine: (validatorId?: string) => post<Block>('/api/chain/mine', validatorId ? { validatorId } : {}),
  pending: () => get<Transaction[]>('/api/transactions/pending'),
  addTransaction: (recipient: string, amount: number, memo?: string) =>
    post<Transaction>('/api/transactions', { recipient, amount, memo }),
  wallet: () => get<Wallet>('/api/wallet'),
  balances: () => get<Record<string, number>>('/api/wallet/balances'),
  consensus: () => get<ConsensusState>('/api/consensus'),
  setConsensus: (strategy: string) => post<{ active: string }>('/api/consensus', { strategy }),
  mintNft: (title: string, description: string, metadataUri: string) =>
    post<Nft>('/api/nft/mint', { title, description, metadataUri }),
  nfts: () => get<Nft[]>('/api/nft'),
  medicalConsent: (patientId: string, granteeId: string, scope: string, granted: boolean) =>
    post<ContractResult>('/api/contracts/medical/consent', { patientId, granteeId, scope, granted }),
  medicalRecords: (patientId: string) =>
    get<ConsentRecord[]>(`/api/contracts/medical/${encodeURIComponent(patientId)}`),
  agriEvent: (batchId: string, stage: string, actor: string, location: string, details: string) =>
    post<ContractResult>('/api/contracts/agri/event', { batchId, stage, actor, location, details }),
  agriTrace: (batchId: string) =>
    get<SupplyChainEvent[]>(`/api/contracts/agri/${encodeURIComponent(batchId)}`),
  p2pConnect: (url: string) => post<ConnectResult>('/api/p2p/connect', { url }),
  p2pSync: () => post<SyncResult>('/api/p2p/sync'),
}
