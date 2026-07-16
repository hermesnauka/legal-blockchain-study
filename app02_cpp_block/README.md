# LegalChain — Educational Post-Quantum Blockchain 3.0 (C++20 / Drogon + React)

A compliant, educational distributed ledger MVP: a real general ledger (not a mock) with
post-quantum cryptography (ML-DSA / ML-KEM via liboqs), hot-swappable consensus
(PoW / PoS / BFT), smart contracts for medicine and agriculture, NFT minting with
SSI-style creator identity, crowdfunding, and encrypted two-node P2P synchronization.
The UI is a live demonstration and interactive encyclopedia, fully bilingual (🇬🇧/🇵🇱
flag switcher in the header). This is the C++ port of `app01_java_block`; see that
directory's README for the reference Java/Spring Boot implementation.

- SDLC documents (architecture, epics, requirements): [`docs/sdlc/`](docs/sdlc/)
- Backend: C++20, Drogon (HTTP + WebSocket), liboqs (PQC), OpenSSL EVP, in-memory ledger
- Frontend: React 18 + Vite + TypeScript in [`frontend/`](frontend/) (shared design with app01;
  only the backend proxy port differs)

## Prerequisites

```bash
sudo apt install cmake libdrogon-dev libssl-dev
```

`liboqs` (the PQC library) and the GoogleTest suite are not packaged in apt — CMake
fetches and builds both automatically from source (`FetchContent`) the first time you
configure the project; this needs a working internet connection on first build only.
`ninja-build` is optional (faster incremental builds); without it CMake falls back to
Unix Makefiles.

## Quick start

ATTENTION!!! Remember about hiding secrets and passwords in Vaults, secured .env file (not commited) or environment variables (like "${POSTGRES_PASSWORD}") to keep them in secret.
In this manual secrets and passwords are not secured in such proper way: only for educational purpose and better understanding what is going on. Learn how to hide and keep in secret in Vaults... You can run this open-source code at your own risk. Caveat emptor.

### 1. Build

```bash
cmake -B build            # add -G Ninja if you have ninja-build installed
cmake --build build -j
```

Run the tests:

```bash
ctest --test-dir build --output-on-failure
```

### 2. Backend node (port 8090)

```bash
./build/legalchain_node
```

### 3. Frontend (port 5174)

```bash
cd frontend
npm install
npm run dev
```

Open http://localhost:5174 — the Vite dev server proxies `/api` and `/ws` to the
backend on :8090. Use the 🇬🇧/🇵🇱 buttons in the header to switch the entire UI
language; the choice persists across reloads.

### 4. Two-node P2P demo

Start a second node in another terminal:

```bash
./build/legalchain_node --port 8091 --node-name node-B
```

Then on the **P2P Network** page (or via REST) connect and sync:

```bash
curl -X POST http://localhost:8091/api/p2p/connect \
     -H 'Content-Type: application/json' \
     -d '{"url":"ws://localhost:8090/ws/p2p"}'
# the ML-DSA + ML-KEM handshake completes in the background (~1 s), then:
curl -X POST http://localhost:8091/api/p2p/sync
```

Both nodes now report the same chain head — the channel is encrypted with
AES-256-GCM under an ML-KEM-768 (Kyber) encapsulated session key, and each peer
proves key possession with an ML-DSA (Dilithium) signature: MITM-resistant and
quantum-resistant, while peers remain pseudonymous (key fingerprints only).

## Guided tour

| Page | What it demonstrates |
|---|---|
| Ledger | Live blocks, hash → previousHash linkage, Merkle roots, mempool, signed transfers, mining rewards (tokenomics), chain validation |
| P2P Network | Two-node sync, the post-quantum handshake, why it defeats MITM |
| Consensus | Encyclopedia of PoW, PoS, Hybrid, DPoS, PoReputation, PoUtility, BFT, PoH, PoET — with live switching of PoW/PoS/BFT |
| PQC Security | Bitcoin vs. this app, Shor/Grover, harvest-now-decrypt-later, NIST FIPS 203/204, attack scenarios |
| Use Cases | DRM, logistics, medicine, agriculture, higher education, democratic voting, communities |
| Smart Contracts | Medical consent (GDPR-aligned, no clinical data on-chain) and agri supply-chain trace — live |
| NFT Studio | Mint post-quantum-signed ownership certificates; SSI explainer |
| Crowdfunding | Campaign-tagged transfers aggregated into an auditable dashboard from chain data |

## Test suite scope note

Two of the six GoogleTest files (`ApiContractIntegrationTest`, `TwoNodeSyncIntegrationTest`)
drive the same real object graph (`config::AppContext`) and the same real cryptography as the
Java port's equivalent tests, but do **not** go through a live HTTP/WebSocket server: Drogon
only supports one `drogon::app()` singleton per process, so two independently-listening node
processes (or a real HTTP client round-trip) can't run inside one test binary the way two Spring
Boot `ApplicationContext`s can share one JVM. The actual wire transport is exercised by hand via
the two-node curl demo above. See the comment at the top of each of those two test files for
details.
