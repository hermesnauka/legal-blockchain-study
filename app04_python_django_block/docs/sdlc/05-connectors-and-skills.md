# LegalChain — Connectors & Skills (SDLC Doc 05)

**Author role:** Enterprise System Architect & Product Owner.
Integration connectors required by the MVP (and their evolution path), plus the AI/agent "Skills" the implementation team needs.

---

## 1. Integration connectors

### 1.1 MVP connectors (implemented now)

| Connector | Technology | Purpose | Security/compliance notes |
|---|---|---|---|
| **P2P node channel** | Spring WebSocket (`/ws/p2p`), virtual-thread client | Full-ledger synchronization between two remote nodes; block/tx gossip | ML-DSA-signed handshake, ML-KEM-768 session encapsulation, AES-256-GCM frames; pseudonymous fingerprints only (GDPR-clean) |
| **UI event stream** | WebSocket (`/ws/events`) | Push `BLOCK_ADDED` / `TX_ADDED` / `PEER_CONNECTED` / `CHAIN_REPLACED` / `CONSENSUS_CHANGED` to the SPA for live visualization | Read-only event fan-out; no secrets in frames |
| **BouncyCastle PQC provider** | `org.bouncycastle:bcprov-jdk18on` (JCA provider) | ML-DSA (FIPS 204) signatures, ML-KEM (FIPS 203) KEM, SHA3-256 | Registered once at startup; the single source of crypto primitives — no hand-rolled crypto |
| **REST API** | Spring Web (JSON), contract v1 (doc 03) | Commands & queries: chain, wallet, NFT, contracts, consensus, P2P control | CORS restricted to SPA origin; input validation on every mutation |
| **i18n connector** | `GET /api/i18n/{lang}` ← `messages_en/pl.properties` | Serves both dictionaries backing the mandatory 🇬🇧/🇵🇱 flag switcher | Both languages ship in one artifact; no external translation service |
| **IPFS connector (stub)** | `metadataUri` field on NFT mint | NFT metadata reference by CID/URI; MVP stores the URI on-chain only | Off-chain metadata keeps personal data & large payloads off the ledger (GDPR) |

### 1.2 Phase-2+ connectors (designed for, not implemented)

| Connector | Technology | Trigger to adopt |
|---|---|---|
| **IPFS (real)** | `java-ipfs-http-client` / Helia gateway | NFT metadata pinning & retrieval instead of URI stub |
| **GraphQL** | Spring for GraphQL | When frontend needs flexible ledger queries (per-address history, contract state slices) |
| **gRPC** | grpc-java + protobuf | >2 nodes or cross-language peers; streaming block gossip at scale |
| **DID/SSI wallet** | W3C DID + Verifiable Credentials (EUDI Wallet / eIDAS 2.0) | Replace fingerprint pseudonyms with standards-based self-sovereign identity |
| **EBSI** | European Blockchain Services Infrastructure APIs | Diploma/credential verification use case goes beyond educational mock |
| **ZKP library** | e.g. zk-SNARK toolkit (ingonyama/gnark-style or Java bindings) | Upgrade ZKP-*style* pseudonymous auth to actual zero-knowledge proofs (voting, consent) |
| **Persistence** | PostgreSQL/RocksDB snapshotting | Chain survives node restarts; audit-grade retention |

## 2. Agent "Skills" (AI capabilities for the implementation team)

Reusable, scoped skill definitions — each maps to a recurring engineering activity in this project. (Course tip: after a working session that exercises one of these, distill it into a real Claude Code skill.)

| Skill | Scope |
|---|---|
| `quantum-crypto-engineering` | Selecting/wiring NIST PQC primitives via BouncyCastle JCA: ML-DSA parameter sets, ML-KEM via `javax.crypto.KEM`, HKDF key derivation, GCM nonce discipline; reviewing crypto code for misuse (key reuse, nonce reuse, unauthenticated handshakes). |
| `spring-boot-ledger-management` | Spring Boot 3.5 idioms for ledger nodes: records-based domain, virtual-thread configuration, WebSocket endpoints, thread-safe mempool/chain, REST contract v1 conformance, integrity test design. |
| `react-dapp-visualization` | Building the live ledger UI: WS-driven state, animated block/hash linkage, consensus/PQC educational components, accessible accordions/tooltips. |
| `consensus-simulation` | Implementing/verifying Strategy-pattern consensus (PoW difficulty tuning, PoS weighted selection, PBFT vote simulation) and authoring accurate encyclopedia content for the 9 mechanisms. |
| `compliance-audit` | Checking GDPR (no personal data on-chain), eIDAS 2.0 (SSI trajectory), MiCA (closed-loop token) claims against the actual code; writing the "why this is compliant" Javadoc. |
| `i18n-content-authoring` | Maintaining parallel EN/PL dictionaries and long-form educational texts; enforcing identical key sets; quality Polish technical translation for the flag switcher requirement. |

## 3. Connector topology

```mermaid
flowchart LR
    SPA[React SPA 🇬🇧/🇵🇱] -->|REST v1| API[Spring Boot API]
    SPA <-->|WS /ws/events| API
    API --> CORE[Ledger Core + Contracts]
    CORE --> BC[BouncyCastle PQC]
    API <-->|WS /ws/p2p<br/>ML-KEM + AES-GCM| PEER[Remote Node]
    CORE -.->|metadataUri stub| IPFS[(IPFS — Phase 2)]
    CORE -.-> GQL[GraphQL — Phase 2]
    CORE -.-> DID[DID/SSI · EBSI — Phase 2]
```
