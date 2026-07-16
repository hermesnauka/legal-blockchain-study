# LegalChain — Python / Django (app04)

A compliant, educational post-quantum Blockchain 3.0 ledger MVP: a real general ledger
(not a mock) with post-quantum cryptography — ML-DSA-65 signatures (`dilithium-py`,
FIPS 204), ML-KEM-768 key encapsulation (`kyber-py`, FIPS 203), SHA3-256 hashing
(`hashlib`) and AES-256-GCM channel encryption (the `cryptography` package) —
hot-swappable consensus (PoW / PoS / BFT strategy pattern), smart contracts for
medicine and agriculture, NFT minting with SSI-style creator identity, crowdfunding,
and encrypted two-node P2P WebSocket synchronization with a PQC handshake.
The UI is a live demonstration and interactive encyclopedia (React 18 + Vite +
TypeScript), fully bilingual (🇬🇧/🇵🇱 flag switcher in the header). This is the
Python/Django port of `app01_java_block`; see that directory's README for the
reference Java/Spring Boot implementation.

The PQC packages are **pure-Python NIST reference implementations** — deliberately
slower than BouncyCastle/liboqs native bindings, but dependency-light and readable:
you can step through the actual lattice math, which is the point of this course app.

- SDLC documents (architecture, epics, requirements): [`docs/sdlc/`](docs/sdlc/)
- Backend: Python 3.14, Django 6 + Channels + Daphne (ASGI; `runserver` serves
  WebSockets), single `ledger` app, in-memory state (no DB models)
- Frontend: React 18 + Vite + TypeScript in [`frontend/`](frontend/) (shared design
  with app01; only the backend proxy port differs)

## Prerequisites

- **Python 3.14** (with `venv`).
- **Node.js 20+** (for the frontend).

## Quick start

ATTENTION!!! Remember about hiding secrets and passwords in Vaults, secured .env file (not commited) or environment variables (like "${POSTGRES_PASSWORD}") to keep them in secret.
In this manual secrets and passwords are not secured in such proper way: only for educational purpose and better understanding what is going on. Learn how to hide and keep in secret in Vaults... You can run this open-source code at your own risk. Caveat emptor.

### 1. Setup (virtualenv + dependencies)

```bash
python3 -m venv .venv
.venv/bin/pip install django daphne channels dilithium-py kyber-py cryptography websockets
# or equivalently:
.venv/bin/pip install -r requirements.txt
```

### 2. Run the nodes

```bash
# node-A (port 8110)
LEGALCHAIN_NODE_NAME=node-A LEGALCHAIN_PORT=8110 .venv/bin/python manage.py runserver 8110

# node-B (port 8111) — in another terminal
LEGALCHAIN_NODE_NAME=node-B LEGALCHAIN_PORT=8111 .venv/bin/python manage.py runserver 8111
```

`LEGALCHAIN_PORT` only feeds the informational `port` field returned by `GET /api/node` —
set it to match whatever port you pass to `runserver`/`daphne`, since Django itself
doesn't read its own bind port back out of that value.

| Component | Port |
|---|---|
| node-A (Django/Daphne backend) | 8110 |
| node-B (second node instance) | 8111 |
| Vite frontend dev server | 5176 |

Configuration lives in the `LEGALCHAIN` dict of
[`legalchain/settings.py`](legalchain/settings.py) (node name `node-A`, default
consensus `POS`, tokenomics: block reward 50 LGC, halving interval 100, max supply
21,000 LGC); environment variables such as `LEGALCHAIN_NODE_NAME` override it per node.

### 3. Build & test

```bash
.venv/bin/python manage.py test
```

### 4. Frontend (port 5176)

```bash
cd frontend
npm install
npm run dev
```

Open http://localhost:5176 — the Vite dev server proxies `/api` and `/ws` to the
backend on :8110. Use the 🇬🇧/🇵🇱 buttons in the header to switch the entire UI
language; the choice persists across reloads. Production build: `npm run build`
(point it at another backend with `VITE_API_URL`, default `http://localhost:8110`).

### 5. Two-node P2P demo

With node-A and node-B running, connect and sync on the **P2P Network** page (or via REST):

```bash
curl -X POST http://localhost:8110/api/p2p/connect \
     -H 'Content-Type: application/json' \
     -d '{"url":"ws://localhost:8111/ws/p2p"}'
# the ML-DSA + ML-KEM handshake completes in the background (~1 s), then
# mine a block on one node:
curl -X POST http://localhost:8110/api/chain/mine -H 'Content-Type: application/json' -d '{}'
# and sync the other:
curl -X POST http://localhost:8110/api/p2p/sync
# verify: both report the same chain length, and the chain validates
curl http://localhost:8110/api/chain
curl http://localhost:8111/api/chain
curl http://localhost:8110/api/chain/validate   # -> {"valid":true, ...}
```

Both nodes now report the same chain head — the channel is encrypted with
AES-256-GCM under an ML-KEM-768 (Kyber) encapsulated session key, and each peer
proves key possession with an ML-DSA (Dilithium) signature: MITM-resistant and
quantum-resistant, while peers remain pseudonymous (key fingerprints only).

## Project structure

```
manage.py                  # Django entry point (runserver serves HTTP + WebSockets via Daphne)
requirements.txt           # django, daphne, channels, dilithium-py, kyber-py, cryptography, websockets
legalchain/                # Django project: settings.py (LEGALCHAIN config dict), asgi.py, urls.py
ledger/                    # The single Django app — all in-memory, no DB models
  core.py                  #   Block/Transaction frozen dataclasses, chain, mempool, tokenomics
  crypto.py                #   ML-DSA-65, ML-KEM-768, SHA3-256, AES-256-GCM (dilithium-py / kyber-py / cryptography)
  consensus.py             #   Strategy pattern: PoW / PoS / BFT, hot-swappable
  ...                      #   Smart contracts, NFT, P2P (/ws/p2p handshake + sync), API views, /ws/events consumer
  tests/                   #   Django TestCase suites (see below)
i18n/                      # messages_en.json / messages_pl.json → GET /api/i18n/{lang}
frontend/                  # React 18 + Vite + TypeScript SPA (port 5176)
docs/sdlc/                 # architecture, epics, backend/frontend requirements, checklist
```

## Tests

`.venv/bin/python manage.py test` runs six suites:

- **blockchain** — chain integrity, Merkle roots, tamper detection, tokenomics/halving.
- **pqc_crypto** — ML-DSA sign/verify roundtrip, ML-KEM shared-secret agreement, SHA3/AES-GCM.
- **consensus** — PoW/PoS/BFT seal & verify, hot-swap, PBFT vote thresholds.
- **contracts** — medical consent grant/revoke and agri supply-chain replay from the ledger.
- **api_contract** — every REST endpoint against API contract v1.
- **two_node_sync** — two in-process nodes: PQC handshake, encrypted sync, longest-valid-chain adoption.

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
