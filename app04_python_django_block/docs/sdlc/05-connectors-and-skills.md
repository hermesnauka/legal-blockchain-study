# LegalChain — Connectors & Skills (SDLC Doc 05)

**Author role:** Enterprise System Architect & Product Owner.
Integration connectors required by the MVP (and their evolution path), plus the AI/agent "Skills" the implementation team needs.

---

## 1. Integration connectors

### 1.1 MVP connectors (implemented now)

| Connector | Technology | Purpose | Security/compliance notes |
|---|---|---|---|
| **P2P node channel** | Django Channels async consumer (`/ws/p2p`) on Daphne ASGI; outbound client via the `websockets` library on a background thread | Full-ledger synchronization between two remote nodes; block/tx gossip | ML-DSA-signed handshake, ML-KEM-768 session encapsulation, AES-256-GCM frames; pseudonymous fingerprints only (GDPR-clean) |
| **UI event stream** | WebSocket (`/ws/events`, Channels consumer) | Push `BLOCK_ADDED` / `TX_ADDED` / `PEER_CONNECTED` / `CHAIN_REPLACED` / `CONSENSUS_CHANGED` to the SPA for live visualization | Read-only event fan-out; no secrets in frames |
| **PQC provider** | `dilithium-py` (ML-DSA, FIPS 204) + `kyber-py` (ML-KEM, FIPS 203) + `cryptography` (AES-256-GCM) + `hashlib` SHA3-256 | ML-DSA signatures, ML-KEM KEM, SHA3-256 hashing, authenticated channel encryption | Pure-Python NIST reference implementations (readable, dependency-light; slower than native liboqs/BouncyCastle) — the single source of crypto primitives, no hand-rolled crypto |
| **REST API** | Django views (JSON), contract v1 (doc 03) | Commands & queries: chain, wallet, NFT, contracts, consensus, P2P control | CORS restricted to SPA origin; input validation on every mutation |
| **i18n connector** | `GET /api/i18n/{lang}` ← `i18n/messages_{en,pl}.json` | Serves both dictionaries backing the mandatory 🇬🇧/🇵🇱 flag switcher | Both languages ship in one artifact; no external translation service |
| **IPFS connector (stub)** | `metadataUri` field on NFT mint | NFT metadata reference by CID/URI; MVP stores the URI on-chain only | Off-chain metadata keeps personal data & large payloads off the ledger (GDPR) |

### 1.2 Phase-2+ connectors (designed for, not implemented)

| Connector | Technology | Trigger to adopt |
|---|---|---|
| **IPFS (real)** | `ipfshttpclient` / Helia gateway | NFT metadata pinning & retrieval instead of URI stub |
| **GraphQL** | `graphene-django` or `strawberry` | When frontend needs flexible ledger queries (per-address history, contract state slices) |
| **gRPC** | `grpcio` + protobuf | >2 nodes or cross-language peers; streaming block gossip at scale |
| **DID/SSI wallet** | W3C DID + Verifiable Credentials (EUDI Wallet / eIDAS 2.0) | Replace fingerprint pseudonyms with standards-based self-sovereign identity |
| **EBSI** | European Blockchain Services Infrastructure APIs | Diploma/credential verification use case goes beyond educational mock |
| **ZKP library** | e.g. zk-SNARK toolkit (ingonyama/gnark-style or Python bindings) | Upgrade ZKP-*style* pseudonymous auth to actual zero-knowledge proofs (voting, consent) |
| **Persistence** | PostgreSQL (`psycopg`)/RocksDB snapshotting | Chain survives node restarts; audit-grade retention |

## 2. Agent "Skills" (AI capabilities for the implementation team)

Reusable, scoped skill definitions — each maps to a recurring engineering activity in this project. (Course tip: after a working session that exercises one of these, distill it into a real Claude Code skill.)

| Skill | Scope |
|---|---|
| `quantum-crypto-engineering` | Selecting/wiring NIST PQC primitives in Python: ML-DSA via `dilithium_py.ml_dsa.ML_DSA_65`, ML-KEM via `kyber_py.ml_kem.ML_KEM_768`, AES-256-GCM via `cryptography`'s `AESGCM`, HKDF key derivation, GCM nonce discipline; reviewing crypto code for misuse (key reuse, nonce reuse, unauthenticated handshakes). |
| `django-ledger-management` | Django 6 + Channels idioms for ledger nodes: frozen-dataclass domain, in-memory app state (no DB models), async WebSocket consumers on Daphne ASGI, thread-safe mempool/chain, REST contract v1 conformance, integrity test design with `manage.py test`. |
| `react-dapp-visualization` | Building the live ledger UI: WS-driven state, animated block/hash linkage, consensus/PQC educational components, accessible accordions/tooltips. |
| `consensus-simulation` | Implementing/verifying Strategy-pattern consensus (PoW difficulty tuning, PoS weighted selection, PBFT vote simulation) and authoring accurate encyclopedia content for the 9 mechanisms. |
| `compliance-audit` | Checking GDPR (no personal data on-chain), eIDAS 2.0 (SSI trajectory), MiCA (closed-loop token) claims against the actual code; writing the "why this is compliant" docstrings. |
| `i18n-content-authoring` | Maintaining parallel EN/PL dictionaries and long-form educational texts; enforcing identical key sets; quality Polish technical translation for the flag switcher requirement. |

## 3. Connector topology

```mermaid
flowchart LR
    SPA[React SPA 🇬🇧/🇵🇱] -->|REST v1| API[Django 6 + Channels API]
    SPA <-->|WS /ws/events| API
    API --> CORE[Ledger Core + Contracts]
    CORE --> BC[dilithium-py / kyber-py PQC]
    API <-->|WS /ws/p2p<br/>ML-KEM + AES-GCM| PEER[Remote Node]
    CORE -.->|metadataUri stub| IPFS[(IPFS — Phase 2)]
    CORE -.-> GQL[GraphQL — Phase 2]
    CORE -.-> DID[DID/SSI · EBSI — Phase 2]
```
