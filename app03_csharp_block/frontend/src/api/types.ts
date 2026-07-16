export type TxType =
  | 'TRANSFER'
  | 'REWARD'
  | 'NFT_MINT'
  | 'CONTRACT_MEDICAL'
  | 'CONTRACT_AGRI'

export interface Transaction {
  id: string
  timestamp: number
  sender: string
  recipient: string
  amount: number
  type: TxType
  payload: Record<string, string> | null
  senderPublicKey: string | null
  signature: string | null
}

export interface Block {
  index: number
  timestamp: number
  previousHash: string
  hash: string
  merkleRoot: string
  validatorId: string
  consensusType: string
  proof: string
  nonce: number
  transactions: Transaction[]
}

export interface ChainInfo {
  length: number
  valid: boolean
  blocks: Block[]
}

export interface ChainValidation {
  valid: boolean
  message: string
}

export interface Peer {
  nodeId: string
  url: string
}

export interface NodeInfo {
  nodeId: string
  name: string
  port: number
  consensus: string
  peers: Peer[]
  chainLength: number
}

export interface Wallet {
  address: string
  algorithm: string
  publicKey: string
  fingerprint: string
  balance: number
}

export interface ConsensusOption {
  name: string
  description: string
}

export interface ConsensusState {
  active: string
  available: ConsensusOption[]
}

export interface Nft {
  tokenId: string
  title: string
  description: string
  metadataUri: string
  creator: string
  mintedAt: number
  txId: string
}

export interface ContractResult {
  success?: boolean
  message?: string
  txId?: string
  [key: string]: unknown
}

export interface ConsentRecord {
  patientId: string
  granteeId: string
  scope: string
  granted: boolean
  timestamp: number
  txId?: string
}

export interface SupplyChainEvent {
  batchId: string
  stage: string
  actor: string
  location: string
  details: string
  timestamp: number
  txId?: string
}

export interface ConnectResult {
  connected: boolean
  peer?: string
}

export interface SyncResult {
  result: string
  chainLength: number
}

export type LedgerEventType =
  | 'BLOCK_ADDED'
  | 'TX_ADDED'
  | 'PEER_CONNECTED'
  | 'CHAIN_REPLACED'
  | 'CONSENSUS_CHANGED'

export interface LedgerEvent {
  type: LedgerEventType
  data: unknown
}
