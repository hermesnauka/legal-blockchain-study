# LegalChain — C# / ASP.NET Core (app03)

A compliant, educational post-quantum Blockchain 3.0 ledger MVP: a real general ledger
(not a mock) with post-quantum cryptography — ML-DSA-65 signatures, ML-KEM-768 key
encapsulation, SHA3-256 hashing (all via `BouncyCastle.Cryptography`) and AES-256-GCM
channel encryption — hot-swappable consensus (PoW / PoS / BFT strategy pattern), smart
contracts for medicine and agriculture, NFT minting with SSI-style creator identity,
crowdfunding, and encrypted two-node P2P WebSocket synchronization with a PQC handshake.
The UI is a live demonstration and interactive encyclopedia (React 18 + Vite +
TypeScript), fully bilingual (🇬🇧/🇵🇱 flag switcher in the header). This is the C#/.NET
port of `app01_java_block`; see that directory's README for the reference Java/Spring
Boot implementation.

- SDLC documents (architecture, epics, requirements): [`docs/sdlc/`](docs/sdlc/)
- Backend: C# on .NET 10 (LTS), ASP.NET Core, records, async/await, in-memory ledger
- Frontend: React 18 + Vite + TypeScript in [`frontend/`](frontend/) (shared design with app01;
  only the backend proxy port differs)

## Prerequisites

- **.NET SDK 10** — user-local install, no root required:

```bash
curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 10.0 --install-dir "$HOME/.dotnet"
export DOTNET_ROOT="$HOME/.dotnet"
export PATH="$HOME/.dotnet:$PATH"
```

- **Node.js 20+** (for the frontend).

## Quick start

ATTENTION!!! Remember about hiding secrets and passwords in Vaults, secured .env file (not commited) or environment variables (like "${POSTGRES_PASSWORD}") to keep them in secret.
In this manual secrets and passwords are not secured in such proper way: only for educational purpose and better understanding what is going on. Learn how to hide and keep in secret in Vaults... You can run this open-source code at your own risk. Caveat emptor.

### 1. Build & test

```bash
dotnet build
dotnet test
```

### 2. Run the nodes

```bash
# node-A (port 8100)
dotnet run --project src/LegalChain

# node-B (port 8101) — in another terminal
dotnet run --project src/LegalChain -- --port 8101 --node-name node-B
```

| Component | Port |
|---|---|
| node-A (ASP.NET Core backend) | 8100 |
| node-B (second node instance) | 8101 |
| Vite frontend dev server | 5175 |

Configuration lives in the `"LegalChain"` section of
[`src/LegalChain/appsettings.json`](src/LegalChain/appsettings.json) (port, node name,
default consensus strategy, tokenomics: block reward 50 LGC, halving interval, max
supply); `--port` and `--node-name` override it per node.

### 3. Frontend (port 5175)

```bash
cd frontend
npm install
npm run dev
```

Open http://localhost:5175 — the Vite dev server proxies `/api` and `/ws` to the
backend on :8100. Use the 🇬🇧/🇵🇱 buttons in the header to switch the entire UI
language; the choice persists across reloads. Production build: `npm run build`
(point it at another backend with `VITE_API_URL`, default `http://localhost:8100`).

### 4. Two-node P2P demo

With node-A and node-B running, connect and sync on the **P2P Network** page (or via REST):

```bash
curl -X POST http://localhost:8100/api/p2p/connect \
     -H 'Content-Type: application/json' \
     -d '{"url":"ws://localhost:8101/ws/p2p"}'
# the ML-DSA + ML-KEM handshake completes in the background (~1 s), then
# mine a block on one node:
curl -X POST http://localhost:8100/api/chain/mine -H 'Content-Type: application/json' -d '{}'
# and sync the other:
curl -X POST http://localhost:8101/api/p2p/sync
# verify: both report the same chain length, and the chain validates
curl http://localhost:8100/api/chain
curl http://localhost:8101/api/chain
curl http://localhost:8101/api/chain/validate   # -> {"valid":true, ...}
```

Both nodes now report the same chain head — the channel is encrypted with
AES-256-GCM under an ML-KEM-768 (Kyber) encapsulated session key, and each peer
proves key possession with an ML-DSA (Dilithium) signature: MITM-resistant and
quantum-resistant, while peers remain pseudonymous (key fingerprints only).

## Project structure

```
LegalChain.slnx
src/LegalChain/            # ASP.NET Core node (namespaces LegalChain.*)
  Core/                    #   Block/Transaction records, chain, mempool, tokenomics
  Crypto/                  #   ML-DSA-65, ML-KEM-768, SHA3-256, AES-256-GCM (BouncyCastle.Cryptography)
  Consensus/               #   Strategy pattern: PoW / PoS / BFT, hot-swappable
  Contracts/               #   MedicalConsentContract, AgriSupplyChainContract
  Nft/                     #   NFT minting + gallery
  P2p/                     #   /ws/p2p node link, PQC handshake, longest-valid-chain sync
  Api/                     #   MVC controllers (API contract v1) + /ws/events browser feed
  Config/                  #   appsettings binding, DI wiring (LegalChainApp)
  i18n/                    #   messages_en.json / messages_pl.json → GET /api/i18n/{lang}
tests/LegalChain.Tests/    # xUnit test suites
frontend/                  # React 18 + Vite + TypeScript SPA (port 5175)
docs/sdlc/                 # architecture, epics, backend/frontend requirements, checklist
```

## Tests

`dotnet test` runs six xUnit suites:

- **BlockchainTest** — chain integrity, Merkle roots, tamper detection, tokenomics/halving.
- **PqcCryptoTest** — ML-DSA sign/verify roundtrip, ML-KEM shared-secret agreement, SHA3/AES-GCM.
- **ConsensusStrategyTest** — PoW/PoS/BFT seal & verify, hot-swap, PBFT vote thresholds.
- **ContractsTest** — medical consent grant/revoke and agri supply-chain replay from the ledger.
- **ApiContractIntegrationTest** — every REST endpoint against API contract v1.
- **TwoNodeSyncIntegrationTest** — two in-process nodes: PQC handshake, encrypted sync, longest-valid-chain adoption.

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
