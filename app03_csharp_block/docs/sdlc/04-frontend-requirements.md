# LegalChain — Frontend Requirements (SDLC Doc 04)

**Author role:** Enterprise System Architect (handing off to Lead Frontend & Edu-Tech Dev — ROLE 3).
**Consumes:** the API Contract v1 in [03-backend-requirements.md](03-backend-requirements.md).

---

## 1. Framework decision: React 18 + Vite + TypeScript

| Criterion | React 18 + Vite (chosen) | Vue 3 | Angular 17+ |
|---|---|---|---|
| Live data visualization | Excellent — fine-grained re-render control for animated block/hash views | Good | Good but heavier |
| Ecosystem for dApp/edu UIs | Largest (flow/graph libs, i18n, WS hooks) | Large | Enterprise-centric |
| Learning-curve fit for a course demo | Low; JSX components map 1:1 to educational concepts | Low | High (modules, DI, RxJS) |
| Build/start speed | Vite HMR, instant | Vite HMR | CLI slower |
| Type safety | TypeScript first-class | Good | Built-in |

**Decision:** React 18 + Vite + TypeScript. Angular's ceremony and Vue's smaller dApp ecosystem make React the optimal educational and dApp-visualization choice. No heavy state library — React context + hooks suffice at MVP scale.

## 2. Application architecture

```
frontend/
  src/
    api/            # typed REST client + WebSocket (/ws/events) hook
    i18n/           # LanguageContext, en.ts, pl.ts dictionaries, useT() hook
    components/     # BlockCard, HashLink, TxTable, PeerBadge, FlagSwitcher, Accordion, Tooltip
    views/
      LedgerView    # chain visualization + mining + mempool
      NetworkView   # P2P: connect peer, sync, session security status
      ConsensusView # encyclopedia + live strategy switch
      SecurityView  # PQC vs Bitcoin education
      UseCasesView  # sector dashboards (incl. live medical/agri contracts)
      NftView       # mint + gallery
    App.tsx         # header (logo, nav, FlagSwitcher 🇬🇧/🇵🇱), routing
```

- **State:** server state fetched per view + pushed deltas from `/ws/events` (`BLOCK_ADDED`, `TX_ADDED`, `PEER_CONNECTED`, `CHAIN_REPLACED`, `CONSENSUS_CHANGED`).
- **Styling:** lightweight CSS (custom properties, dark-friendly); no CSS framework dependency required.

## 3. Internationalization (mandatory PL/EN flag switcher)

- Header contains a **flag toggle 🇬🇧/🇵🇱**; one click switches the entire UI — navigation, labels, and all educational long-form texts — without page reload.
- Implementation: `LanguageContext` + `useT(key)` hook; dictionaries `en.ts` / `pl.ts` (flat keys mirroring `GET /api/i18n/{lang}` so backend labels and SPA texts stay consistent).
- Persistence: `localStorage("legalchain.lang")`; default `en`; `<html lang>` attribute updated for accessibility.
- Acceptance: no hard-coded user-visible string outside the dictionaries; both dictionaries have identical key sets (CI-checkable).

## 4. Views

### 4.1 Ledger visualization (General Ledger)
- Horizontal chain of **BlockCards**: index, truncated SHA3-256 hash, `previousHash` visually linked to the predecessor (arrow/link highlight on hover), validator, consensus type, tx count.
- Mempool panel with pending transactions; forms to create a transfer and to mine; live update on WS events (new block animates in ≤ 1 s — US-5.1).
- Chain validity indicator driven by `GET /api/chain/validate`.

### 4.2 P2P network view
- Two-node topology graphic: this node + connected peer(s); connect-by-URL form (`POST /api/p2p/connect`), sync button (`POST /api/p2p/sync`).
- Channel security panel: shows handshake facts — ML-DSA-signed transcript, ML-KEM-768 encapsulation, AES-256-GCM session — and the peer's pseudonymous fingerprint (teaching moment: trust without identity disclosure).

### 4.3 Consensus encyclopedia
Interactive accordions/tooltips for: **PoW, PoS, Hybrid, DPoS, PoReputation, PoUtility, BFT, PoH, PoET**. Each entry: how it works, energy/finality/decentralization trade-offs, where it is used, and an "attack resistance" note. PoW/PoS/BFT entries include a **"Try it live"** button that calls `POST /api/consensus` and shows the strategy stamped into the next mined block.

### 4.4 Cybersecurity & PQC education
- Side-by-side comparison: **Bitcoin (ECDSA/secp256k1, SHA-256, PoW)** vs **LegalChain (ML-DSA-65, SHA3-256, ML-KEM channel)**.
- Explains Shor's algorithm (breaks ECDSA), Grover's algorithm (halves hash strength), "harvest-now-decrypt-later", and how the QKD-inspired per-session key freshness mitigates it. Live data from `GET /api/wallet` (real ML-DSA key sizes make the point tangibly — a Dilithium public key vs a 33-byte ECDSA key).

### 4.5 Business use-case dashboards
| Sector | Content | Backend |
|---|---|---|
| Creative industries / DRM | NFT mint + gallery, provenance chain | live (`/api/nft`) |
| Logistics & supply chain | Batch timeline farm→fork | live (`/api/contracts/agri/*`) |
| Medicine | Consent grant/revoke + consent state table | live (`/api/contracts/medical/*`) |
| Agriculture | Shares agri contract with logistics view | live |
| Higher education | Diploma-as-verifiable-credential explainer + mock verification flow | educational |
| Democracy / voting | Compliant remote voting concept (eligibility via SSI, ballot privacy via ZKP) | educational |
| Communities / social enterprise | DAO-style governance explainer | educational |

Each dashboard: what problem the ledger solves, which compliance rule applies (GDPR/eIDAS/MiCA), and — where live — a working form against the API.

### 4.6 NFT view
Mint form (`title`, `description`, `metadataUri` — labeled as IPFS CID stub) and gallery with owner fingerprints and mint tx links.

## 5. Non-functional requirements

| NFR | Requirement |
|---|---|
| Accessibility | WCAG 2.1 AA intent: keyboard-navigable accordions, `aria-expanded`, contrast ≥ 4.5:1, `lang` attribute follows switcher. |
| Responsiveness | Usable ≥ 360 px; chain view horizontally scrollable on mobile. |
| Resilience | Backend-down state shows a friendly banner (in the active language) with retry; WS auto-reconnect with backoff. |
| Performance | First meaningful render < 2 s dev-mode; WS delta updates instead of polling. |
| Config | Backend base URL via `VITE_API_URL` (default `http://localhost:8080`). |
| Education quality | Every explanatory text exists in **both** EN and PL, written for an intelligent non-specialist; decentralization, SSI and scalability each have a dedicated explainer section. |
