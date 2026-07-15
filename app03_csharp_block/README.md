# LegalChain — Educational Post-Quantum Blockchain 3.0 (Java 21 / Spring Boot + React)

A compliant, educational distributed ledger MVP: a real general ledger (not a mock) with
post-quantum cryptography (ML-DSA / ML-KEM via Bouncy Castle), hot-swappable consensus
(PoW / PoS / BFT), smart contracts for medicine and agriculture, NFT minting with
SSI-style creator identity, crowdfunding, and encrypted two-node P2P synchronization.
The UI is a live demonstration and interactive encyclopedia, fully bilingual (🇬🇧/🇵🇱
flag switcher in the header).

- SDLC documents (architecture, epics, requirements): [`docs/sdlc/`](docs/sdlc/)
- Backend: Spring Boot 3.5 / JDK 21, virtual threads, in-memory ledger
- Frontend: React 18 + Vite + TypeScript in [`frontend/`](frontend/)

## Quick start

ATTENTION!!! Remember about hiding secrets and passwords in Vaults, secured .env file (not commited) or environment variables (like "${POSTGRES_PASSWORD}") to keep them in secret.
In this manual secrets and passwords are not secured in such proper way: only for educational purpose and better understanding what is going on. Learn how to hide and keep in secret in Vaults... You can run this open-source code at your own risk. Caveat emptor.

### 1. Backend node (port 8080)

Requires JDK 21 and Maven.

```bash
mvn spring-boot:run
```

Run the tests:

```bash
mvn test
```

### 2. Frontend (port 5173)

```bash
cd frontend
npm install
npm run dev
```

Open http://localhost:5173 — the Vite dev server proxies `/api` and `/ws` to the
backend on :8080. Use the 🇬🇧/🇵🇱 buttons in the header to switch the entire UI
language; the choice persists across reloads.

### 3. Two-node P2P demo

Start a second node in another terminal:

```bash
mvn spring-boot:run -Dspring-boot.run.arguments="--server.port=8081 --legalchain.node.name=node-B"
```

Then on the **P2P Network** page (or via REST) connect and sync:

```bash
curl -X POST http://localhost:8081/api/p2p/connect \
     -H 'Content-Type: application/json' \
     -d '{"url":"ws://localhost:8080/ws/p2p"}'
# the ML-DSA + ML-KEM handshake completes in the background (~1 s), then:
curl -X POST http://localhost:8081/api/p2p/sync
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
