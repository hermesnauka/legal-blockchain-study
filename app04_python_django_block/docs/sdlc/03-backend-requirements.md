# LegalChain — Backend Requirements (SDLC Doc 03)

**Stack:** Python 3.14 (frozen dataclasses), Django 6 + Channels + Daphne (ASGI), pure-Python NIST PQC (`dilithium-py`, `kyber-py`) + the `cryptography` package, Channels WebSocket consumers.
**Author role:** Enterprise System Architect (handing off to Lead Python Blockchain Engineer — ROLE 2).
**Cross-cutting:** the backend must serve both language dictionaries via `GET /api/i18n/{lang}` so the SPA's PL/EN flag switcher (🇬🇧/🇵🇱) works without redeploy.

---

## 1. Post-Quantum Cryptography architecture

### 1.1 Algorithms (NIST-standardized)

| Concern | Algorithm | Standard | Python implementation |
|---|---|---|---|
| Digital signatures (transactions, blocks, handshake) | ML-DSA-65 (CRYSTALS-Dilithium) | **FIPS 204** | `dilithium-py` — `dilithium_py.ml_dsa.ML_DSA_65` |
| Key encapsulation (P2P session keys) | ML-KEM-768 (CRYSTALS-Kyber) | **FIPS 203** | `kyber-py` — `kyber_py.ml_kem.ML_KEM_768` |
| Hashing (block hash, Merkle root, address derivation) | SHA3-256 | FIPS 202 | `hashlib.sha3_256` (standard library) |
| Symmetric channel | AES-256-GCM | FIPS 197 / SP 800-38D | `cryptography` — `cryptography.hazmat.primitives.ciphers.aead.AESGCM` |

`dilithium-py` and `kyber-py` are **pure-Python NIST reference implementations** — the deliberate educational trade-off: slower than BouncyCastle/liboqs native bindings, but dependency-light, portable, and readable (students can step through the actual lattice math). **Do not** hand-roll PQC or depend on OS-provided experimental PQC — these packages plus `cryptography` are the single source of crypto primitives.

### 1.2 Services

- `PqcSignatureService` — key pair generation, `sign(bytes)`, `verify(pub_key, data, sig)`. Docstrings (PEP 257) must explain why lattice signatures survive Shor's algorithm while ECDSA does not.
- `PqcKeyExchangeService` — ML-KEM encapsulation/decapsulation + HKDF-style derivation of the AES-256-GCM session key. Docstrings must explain the QKD analogy: like QKD, each session gets a *fresh, jointly-derived, never-transmitted* key; unlike QKD, security rests on lattice hardness instead of photon physics.
- `HashUtil` — SHA3-256 hex digests, Merkle root computation.
- Wallet address = `SHA3-256(publicKeyEncoded)` truncated (e.g., first 20 bytes, hex) — pseudonymous SSI-style identifier.

### 1.3 QKD-inspired channel rules

1. New ML-KEM encapsulation per WebSocket session — no long-lived symmetric keys.
2. Handshake transcript signed with ML-DSA by both sides (MITM defense).
3. Unique 96-bit GCM nonce per frame; session terminated on any auth-tag failure (tamper-evidence, mirroring QKD's disturbance detection).

## 2. Blockchain Core & Ledger

- `Block` and `Transaction` are **frozen dataclasses** (`@dataclass(frozen=True)` — immutable). Block hash = SHA3-256 over `index‖timestamp‖previousHash‖merkleRoot‖validatorId‖nonce‖proof`.
- Genesis block is deterministic (fixed timestamp/content) so two fresh nodes share a common root and can sync.
- Mempool: thread-safe pending pool (lock-guarded); transactions verified (signature + funds) on admission.
- Validation: `validate_chain()` re-checks hash linkage, Merkle roots, per-transaction signatures, and consensus proof of every block.
- **Tokenomics:** REWARD transaction per mined block. Initial reward 50 LGC; halves every N blocks (configurable, default 10); hard cap enforced (default 21,000 LGC); balances derived exclusively by replaying the chain.

## 3. Node management

- Each process is one node: identity = wallet fingerprint; configuration via the `LEGALCHAIN` dict in `settings.py` (node name, default consensus, tokenomics) with environment-variable overrides (e.g., `LEGALCHAIN_NODE_NAME=node-B`) plus the `runserver` port argument for the second node (`manage.py runserver 8111`).
- `NodeRegistry` tracks connected peers `{nodeId, url, fingerprint}`.
- `GET /api/node` exposes node id, port, active consensus, peers, chain length.
- Request handling and P2P I/O combine **Django's threading with an asyncio event loop**: Channels consumers are async on the ASGI (Daphne) event loop, while the outbound P2P client runs the `websockets` library on a background thread — the Python analogue of Java virtual threads for cheap per-connection concurrency.

## 4. Consensus — Strategy pattern

```mermaid
classDiagram
    class ConsensusStrategy {
        <<interface>>
        +String name()
        +String description()
        +Block seal(BlockDraft draft)
        +boolean verify(Block block)
    }
    ConsensusStrategy <|.. ProofOfWorkStrategy
    ConsensusStrategy <|.. ProofOfStakeStrategy
    ConsensusStrategy <|.. BftStrategy
    class ConsensusEngine {
        -Map~String,ConsensusStrategy~ strategies
        -ConsensusStrategy active
        +switchTo(name)
        +seal(draft)
    }
    ConsensusEngine o-- ConsensusStrategy
```

**Implemented (executable):**
- **PoW** — leading-zero SHA3 puzzle with modest difficulty (educational, visibly slower).
- **PoS** — validator selected deterministically weighted by on-chain stake (balance); proof records the stake snapshot. Slashing documented as Phase 2.
- **BFT (emphasized)** — simulated 4-validator PBFT round (pre-prepare/prepare/commit); proof records the ≥2f+1 vote set; tolerates f < n/3 faulty validators. Docstrings must explain why BFT finality suits permissioned, compliance-oriented ledgers.

**Documented conceptually (strategy descriptions + frontend encyclopedia):** Hybrid, DPoS, PoReputation, PoUtility, PoH, PoET.

Active strategy is hot-swappable via `POST /api/consensus {strategy}` and reported in each block's `consensusType`.

## 5. Smart Contract engine (Blockchain 3.0)

`SmartContract` interface (abstract base class): `id()`, `execute(ContractContext) → ContractResult`; every execution is recorded as a typed transaction (CONTRACT_MEDICAL / CONTRACT_AGRI) so contract state is fully reconstructible from the ledger.

- **MedicalConsentContract** — consent-based patient history access: grant/revoke `{patientId (pseudonym), granteeId, scope, granted}`. GDPR rule: *only consent decisions and hashes on-chain; never clinical data.* Current consent state = replay of the patient's contract transactions.
- **AgriSupplyChainContract** — supply-chain transparency: append-only events `{batchId, stage (HARVEST→TRANSPORT→PROCESSING→RETAIL), actor, location, details}`; the ledger provides farm-to-fork provenance.

## 6. P2P synchronization (2 remote nodes)

- Transport: **WebSocket** endpoint `/ws/p2p` served by a Django Channels async consumer (Daphne ASGI; `runserver` serves WebSockets directly); outbound client via the `websockets` library on a background thread.
- Handshake per doc 01 §4.2 (ML-DSA signatures + ML-KEM encapsulation → AES-256-GCM channel).
- Protocol messages (encrypted after handshake): `CHAIN_REQUEST`, `CHAIN_RESPONSE`, `NEW_BLOCK`, `NEW_TX`.
- Conflict resolution: **longest fully-valid chain wins**; the adopted chain is completely re-validated (hashes, signatures, proofs) before replacement.
- Docstring requirement (from CLAUDE.md ROLE 2): explain in module/class docstrings how signed-handshake + KEM establishes trust between parties **without revealing full identities** (pseudonymous fingerprints, proof of key possession — a ZKP-flavored property).

## 7. Internationalization

`GET /api/i18n/{lang}` returns the flat string dictionary for `en` or `pl` (backed by `i18n/messages_en.json` / `i18n/messages_pl.json`). Educational long-form texts may live in the SPA bundle, but every backend-sourced label must come from these bundles so the flag switcher covers the whole UI.

## API Contract (agreed, v1)

```
Base URL: http://localhost:8110

GET  /api/node                 -> {nodeId, name, port, consensus, peers:[{nodeId,url}], chainLength}
GET  /api/chain                -> {length, valid, blocks:[Block]}
GET  /api/chain/validate       -> {valid, message}
POST /api/chain/mine           {validatorId?} -> Block
GET  /api/transactions/pending -> [Transaction]
POST /api/transactions         {recipient, amount, memo?} -> Transaction (signed with node wallet ML-DSA key)
GET  /api/wallet               -> {address, algorithm, publicKey, fingerprint, balance}
GET  /api/wallet/balances      -> {address: balance, ...}
GET  /api/consensus            -> {active, available:[{name, description}]}
POST /api/consensus            {strategy} -> {active}
POST /api/nft/mint             {title, description, metadataUri} -> Nft
GET  /api/nft                  -> [Nft]
POST /api/contracts/medical/consent {patientId, granteeId, scope, granted} -> ContractResult
GET  /api/contracts/medical/{patientId} -> [ConsentRecord]
POST /api/contracts/agri/event {batchId, stage, actor, location, details} -> ContractResult
GET  /api/contracts/agri/{batchId} -> [SupplyChainEvent]
POST /api/p2p/connect          {url} -> {connected, peer}
POST /api/p2p/sync             -> {result, chainLength}
GET  /api/i18n/{lang}          -> flat map of UI strings (lang: en|pl)
WS   /ws/events                -> pushes {type: BLOCK_ADDED|TX_ADDED|PEER_CONNECTED|CHAIN_REPLACED|CONSENSUS_CHANGED, data}
WS   /ws/p2p                   -> node-to-node channel (ML-DSA-signed handshake, ML-KEM encapsulation, AES-256-GCM)

Block JSON:       {index, timestamp, previousHash, hash, merkleRoot, validatorId, consensusType, proof, nonce, transactions[]}
Transaction JSON: {id, timestamp, sender, recipient, amount, type: TRANSFER|REWARD|NFT_MINT|CONTRACT_MEDICAL|CONTRACT_AGRI, payload, senderPublicKey, signature}
```

## 8. Non-functional requirements

| NFR | Requirement |
|---|---|
| Code quality | Frozen dataclasses for domain, async Channels consumers + background-thread `websockets` client for I/O, PEP 257 docstrings explaining *why compliant & secure* on every crypto/consensus/P2P module. |
| Testing | Django `TestCase` suites (`python manage.py test`): chain integrity & tamper detection, ML-DSA sign/verify roundtrip, ML-KEM shared-secret agreement, consensus seal/verify, contract replay. |
| Observability | Every ledger mutation emits a `/ws/events` frame; server logs handshake fingerprints (never keys). |
| Performance | Mining PoW difficulty tuned so a block seals < 2 s on a laptop (educational responsiveness — mind the pure-Python PQC/hash cost). |
| Security | No private key ever leaves the process; no personal data on-chain; CORS restricted to the SPA origin in dev. |
