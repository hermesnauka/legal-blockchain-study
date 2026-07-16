# LegalChain — Ready-to-Implement Requirements Checklist (SDLC Doc 06)

**Author role:** Product Owner — this is the concluding, testable requirements list handed to ROLE 2 (backend) and ROLE 3 (frontend), C++ port.
Each requirement: **ID · requirement · acceptance criterion · epic trace** ([doc 02](02-epics-user-stories.md)).

---

## Backend (BE)

| ID | Requirement | Acceptance criterion | Epic |
|---|---|---|---|
| BE-01 | `Block`/`Transaction` as immutable value structs; SHA3-256 block hash over `index‖timestamp‖previousHash‖merkleRoot‖validatorId‖consensusType‖proof‖nonce`; Merkle root over transactions | GoogleTest: mutating any transaction changes the Merkle root and invalidates the chain | E1 |
| BE-02 | Deterministic genesis block identical on every fresh node | Two fresh nodes report the same block-0 hash | E1, E4 |
| BE-03 | Mempool accepts only ML-DSA-verified transactions with sufficient funds (REWARD exempt) | Invalid signature or overdraft → 4xx, not queued | E1 |
| BE-04 | `POST /api/chain/mine` seals mempool + REWARD tx via the active consensus strategy | Response Block matches contract v1; `BLOCK_ADDED` pushed on `/ws/events` | E1 |
| BE-05 | Tokenomics: initial reward 50 LGC, halving every 100 blocks (config), hard supply cap 21,000 LGC | GoogleTest: reward at block 101 = 25 LGC; issuance stops at cap | E1 |
| BE-06 | `GET /api/chain/validate` re-verifies hashes, Merkle roots, signatures, proofs | Tampered in-memory block → `valid=false` with explanatory `message` | E1 |
| BE-07 | Balances derived solely by replaying confirmed transactions | `GET /api/wallet/balances` equals manual replay in test | E1 |
| BE-08 | Consensus Strategy pattern with hot-swap: PoW, PoS, BFT executable; DPoS/Hybrid/PoReputation/PoUtility/PoH/PoET described in `GET /api/consensus.available` | `POST /api/consensus {strategy:"BFT"}` → next block's `consensusType=BFT`; `CONSENSUS_CHANGED` event | E1, E5 |
| BE-09 | BFT strategy simulates a 4-validator quorum (≥3-of-4 attestations recorded in `proof`) | `validate(block)` fails if fewer than 3 valid attestations | E1 |
| BE-10 | `SmartContract` abstract base; executions recorded as CONTRACT_* transactions | Contract state fully reconstructible from chain in test | E1 |
| BE-11 | MedicalConsentContract: grant/revoke consent; **no clinical data on-chain** | `GET /api/contracts/medical/{patientId}` returns consent state derived from chain; payload contains only ids/scope/decision | E1 |
| BE-12 | AgriSupplyChainContract: append-only batch events (stage/actor/location) | `GET /api/contracts/agri/{batchId}` returns chronological farm-to-fork trail | E1 |
| BE-13 | NFT mint as signed `NFT_MINT` transaction with `metadataUri` (IPFS stub); gallery endpoint | Minted NFT lists creator address + `txId`; signature verifiable | E2 |
| BE-14 | Node identity = wallet fingerprint; `GET /api/node` per contract v1 | Two instances (ports 8090/8091) report distinct nodeIds | E4 |
| BE-15 | REST API conforms exactly to **API Contract v1** (doc 03) | Contract-level integration test per endpoint | all |
| BE-16 | PoW mining runs off the Drogon event loop on a dedicated `std::jthread` | Concurrent request during mining is still served promptly; header comment explains the concurrency choice | NFR |
| BE-17 | Doxygen-style comments on all crypto/consensus/P2P classes explain *why compliant & secure* | Review checklist passes | NFR |

## Security & PQC (SEC)

| ID | Requirement | Acceptance criterion | Epic |
|---|---|---|---|
| SEC-01 | liboqs vendored via CMake `FetchContent`; all PQC primitives via liboqs, all hashing/AEAD via OpenSSL EVP (no hand-rolled crypto) | Build links liboqs + OpenSSL; no custom cipher code | E1 |
| SEC-02 | ML-DSA-65 (FIPS 204) wallet keys; sign/verify for every transaction | Roundtrip GoogleTest; corrupted signature fails verify | E1, E2 |
| SEC-03 | Wallet address/fingerprint = `"lc" + SHA3-256(publicKeyEncoded)` truncated — pseudonymous SSI-style identity, no personal data | `GET /api/wallet` exposes address/algorithm/fingerprint only | E2, E4 |
| SEC-04 | P2P handshake: ML-DSA-signed payload + ML-KEM-768 encapsulation → AES-256-GCM session key | Integration test: two nodes derive identical session key; tampered handshake aborts | E4 |
| SEC-05 | QKD-inspired freshness: new KEM encapsulation per session; unique GCM nonce per frame; abort on auth-tag failure | Code review + test: key never reused across sessions | E4 |
| SEC-06 | MITM defense documented in header comments: interceptor cannot substitute the KEM key without breaking the signed handshake | Comment present on handshake classes | E4 |
| SEC-07 | ZKP-style pseudonymous trust: peers learn only fingerprint + proof of key possession | Handshake payload contains no identity attributes | E4 |
| SEC-08 | Private keys never serialized or exposed over any API | Grep/test: no key material in REST/WS payloads or logs | NFR |

## Frontend (FE)

| ID | Requirement | Acceptance criterion | Epic |
|---|---|---|---|
| FE-01 | React 18 + Vite + TypeScript SPA (reused from `app01_java_block/frontend`) consuming API contract v1 (`VITE_API_URL`) | Builds with `npm run build`; runs against a live C++ node with no component code changes | E5 |
| FE-02 | Ledger view: animated block cards with hash→previousHash linkage, mempool, transfer + mine forms, validity badge | New block appears ≤ 1 s after `BLOCK_ADDED` (US-5.1) | E5 |
| FE-03 | Network view: peer connect/sync UI + channel security panel (ML-DSA/ML-KEM/AES-GCM facts, peer fingerprint) | Connecting two local nodes shows peer + `PEER_CONNECTED`; sync shows `CHAIN_REPLACED` | E4, E5 |
| FE-04 | Consensus encyclopedia: accordions for PoW, PoS, Hybrid, DPoS, PoReputation, PoUtility, BFT, PoH, PoET; "try it live" for PoW/PoS/BFT | Each entry has explanation + trade-offs in EN & PL; live switch changes next block's `consensusType` | E5 |
| FE-05 | PQC education view: LegalChain (ML-DSA/SHA3/ML-KEM) vs Bitcoin (ECDSA/SHA-256), Shor/Grover, harvest-now-decrypt-later | Real key sizes rendered from `GET /api/wallet` | E5 |
| FE-06 | Use-case dashboards: DRM/NFT, logistics+agri (live), medicine (live), higher-ed diplomas, democratic voting, communities/social enterprise | Medical & agri forms round-trip through contract endpoints | E5 |
| FE-07 | NFT view: mint form + gallery with provenance | Minted NFT visible after mining with creator fingerprint | E2 |
| FE-08 | Crowdfunding demo: campaign-tagged transfers aggregated in a dashboard | Contributions per campaign computed from chain data | E3 |
| FE-09 | WS auto-reconnect; backend-down banner in active language | Kill backend → banner; restart → recovery | NFR |
| FE-10 | Accessibility: keyboard-navigable accordions, aria attributes, contrast AA, `<html lang>` synced | Manual audit checklist passes | NFR |

## Internationalization (I18N) — mandatory

| ID | Requirement | Acceptance criterion | Epic |
|---|---|---|---|
| I18N-01 | Header **flag switcher 🇬🇧/🇵🇱** toggling the entire UI (labels + educational texts) without reload | One click re-renders all visible strings in the other language | E6 |
| I18N-02 | Choice persisted in `localStorage`; default EN; `<html lang>` updated | Reload keeps the selected language | E6 |
| I18N-03 | EN and PL dictionaries have identical key sets; no user-visible hard-coded strings | Key-set equality test | E6 |
| I18N-04 | Backend serves both dictionaries via `GET /api/i18n/{lang}` from `messages_en.json`/`messages_pl.json` | Endpoint returns flat maps for `en` and `pl` | E6 |

## Definition of Done (MVP)

1. All **Must** stories (doc 02) satisfied; every BE/SEC/FE/I18N acceptance criterion demonstrably true.
2. `ctest --test-dir build` green (chain integrity, PQC roundtrips, consensus, contracts, two-node P2P sync); frontend builds clean.
3. Two-node demo script works: start `:8090` and `:8091` → connect → transact → mine → sync → both chains identical.
4. Every crypto/consensus/P2P class carries compliance-and-security Doxygen comments (BE-17).
5. Full UI verified in both Polish and English via the flag switcher.
