# LegalChain — Epics & User Stories (SDLC Doc 02)

**Author role:** Product Owner
**Format:** *As a [role], I want [action], so that [benefit]* + Given/When/Then acceptance criteria.
**Prioritization:** MoSCoW. **Estimation:** story points (Fibonacci).

Personas: **Learner** (course participant), **Node Operator** (runs a ledger node), **Creator** (mints NFTs), **Patient / Clinician** (medical contract), **Farmer / Inspector** (agri contract), **Backer / Project Owner** (crowdfunding), **Compliance Auditor**.

---

## Epic E1 — Compliant General Ledger Registry

> A functional, tamper-evident blockchain acting as a general ledger with transparent tokenomics.

**US-1.1** — As a **Node Operator**, I want to submit a signed transaction to the mempool, so that value transfers are recorded in a verifiable ledger.
- *Given* a running node with a wallet, *when* I `POST /api/transactions {recipient, amount}`, *then* the transaction is ML-DSA-signed, verified, added to the pending pool, and a `TX_ADDED` event is pushed over `/ws/events`.

**US-1.2** — As a **Node Operator**, I want to seal pending transactions into a block via the active consensus strategy, so that the ledger grows in an auditable way.
- *Given* pending transactions, *when* I `POST /api/chain/mine`, *then* a block with a correct `previousHash`, Merkle root and consensus proof is appended, and a REWARD transaction pays the validator per the tokenomics schedule.

**US-1.3** — As a **Compliance Auditor**, I want to validate the whole chain on demand, so that I can attest integrity without trusting the operator.
- *Given* any chain state, *when* I `GET /api/chain/validate`, *then* I receive `{valid, message}` computed by re-hashing every block and re-verifying every signature; any tampering yields `valid=false` with the offending index.

**US-1.4** — As a **Learner**, I want to see wallet balances derived purely from chain history, so that I understand the UTXO/account model of a general ledger.
- *Given* a chain with REWARD and TRANSFER transactions, *when* I `GET /api/wallet/balances`, *then* balances equal the deterministic replay of all confirmed transactions.

**US-1.5** — As a **Node Operator**, I want block rewards to follow a capped, halving schedule, so that token issuance is transparent and MiCA-friendly (closed-loop utility token).
- *Given* the reward schedule, *when* the chain passes a halving interval, *then* the next REWARD amount is halved and total supply never exceeds the cap.

## Epic E2 — NFT Digital Ownership & SSI Creator Identity

> Creation and certification of digital ownership and creator identity.

**US-2.1** — As a **Creator**, I want to mint an NFT with title, description and a metadata URI (IPFS CID stub), so that my digital work has a certified, timestamped ownership record.
- *Given* a wallet, *when* I `POST /api/nft/mint`, *then* an `NFT_MINT` transaction signed with my ML-DSA key is queued, and after mining the NFT appears in `GET /api/nft` with my address as owner and the mint transaction id.

**US-2.2** — As a **Creator**, I want my identity on the ledger to be a self-sovereign key fingerprint rather than personal data, so that I control disclosure of who I am (SSI) and the ledger stays GDPR-clean.
- *Given* a node wallet, *when* I `GET /api/wallet`, *then* I see `address`, `algorithm=ML-DSA-65`, `publicKey` and `fingerprint`, and no personal data is stored on-chain.

**US-2.3** — As a **Compliance Auditor**, I want every NFT to be traceable to its mint transaction and signer, so that provenance disputes can be settled from the ledger alone.
- *Given* a minted NFT, *when* I inspect its `mintTxId` on-chain, *then* signature verification against `senderPublicKey` succeeds.

## Epic E3 — Crowdfunding Mechanisms

> Token-based crowdfunding demonstrating programmable value flows. *(MVP: modeled as memo-tagged transfers + dashboard; escrow contract in Phase 2.)*

**US-3.1** — As a **Project Owner**, I want to publish a funding goal identified by a campaign tag, so that backers can direct contributions to my project.
- *Given* a campaign tag, *when* backers send TRANSFER transactions with the campaign memo, *then* the dashboard aggregates contributions per campaign from chain data.

**US-3.2** — As a **Backer**, I want my contribution recorded on-chain with my pseudonymous address, so that funding is transparent yet privacy-preserving.
- *Given* a contribution, *when* the block is mined, *then* the amount and campaign tag are publicly auditable while my identity remains a key fingerprint.

**US-3.3 (Phase 2 — Should)** — As a **Backer**, I want funds held by an escrow smart contract released only when the goal is met, so that failed campaigns refund automatically.

## Epic E4 — Two-Node Synchronization with Privacy Preservation

> Ledger synchronization for two remote users, with ZKP-style pseudonymous trust.

**US-4.1** — As a **Node Operator**, I want to connect my node to a remote peer by URL, so that both nodes share one ledger.
- *Given* two running nodes, *when* I `POST /api/p2p/connect {url}`, *then* the ML-DSA-signed / ML-KEM-encapsulated handshake (doc 01 §4.2) completes, the peer appears in `GET /api/node`, and a `PEER_CONNECTED` event is pushed.

**US-4.2** — As a **Node Operator**, I want the shorter valid chain to adopt the longer valid chain, so that both parties converge on one truth without a central arbiter.
- *Given* peered nodes with divergent lengths, *when* I `POST /api/p2p/sync`, *then* the longer chain is fully re-validated and adopted, and a `CHAIN_REPLACED` event is pushed.

**US-4.3** — As a **privacy-conscious Operator**, I want to prove I control my node key without revealing my identity, so that trust is established with zero knowledge of who I am.
- *Given* a handshake, *when* the peer verifies my ML-DSA signature over the transcript, *then* it has proof of key possession while learning only my pseudonymous fingerprint — nothing about my real identity.

**US-4.4** — As a **Node Operator**, I want all sync traffic encrypted with a fresh per-session key, so that recorded traffic cannot be decrypted later, even by a quantum adversary.
- *Given* an established session, *when* frames are exchanged, *then* they are AES-256-GCM encrypted under an ML-KEM-768-derived key that is never reused across sessions.

## Epic E5 — Educational Module

**US-5.1** — As a **Learner**, I want a live visualization of blocks, hashes, transactions and the P2P link, so that abstract concepts become observable behavior of real code.
- *Given* the SPA connected to `/ws/events`, *when* a block is mined or a peer syncs, *then* the chain view animates the new block with its hash linkage within 1 s.

**US-5.2** — As a **Learner**, I want an interactive encyclopedia of consensus mechanisms (PoW, PoS, Hybrid, DPoS, PoReputation, PoUtility, BFT, PoH, PoET), so that I can compare their trade-offs.
- *Given* the consensus view, *when* I expand a mechanism, *then* I see an accessible explanation, pros/cons, and (for PoW/PoS/BFT) a "try it live" switch that changes the node's active strategy.

**US-5.3** — As a **Learner**, I want a PQC education view comparing LegalChain's ML-DSA/ML-KEM with Bitcoin's ECDSA/secp256k1, so that I understand the quantum threat and its mitigation.

**US-5.4** — As a **Learner**, I want use-case dashboards (DRM, logistics, medicine, agriculture, diplomas, voting, communities), so that I see business value, not just mechanics. Medical and agri dashboards must be backed by live contract endpoints.

## Epic E6 — PL/EN Language Switcher (mandatory)

**US-6.1** — As a **Learner**, I want to switch the entire UI between Polish and English with a single flag toggle (🇬🇧/🇵🇱) in the header, so that I can learn in my preferred language.
- *Given* any view, *when* I click the flag switcher, *then* all UI strings and educational texts re-render in the selected language without reload, the choice persists in `localStorage`, and `GET /api/i18n/{lang}` serves both dictionaries.

---

## MoSCoW & Estimates

| Story | Priority | Points |
|---|---|---|
| US-1.1 Signed transactions | Must | 5 |
| US-1.2 Mining + reward | Must | 5 |
| US-1.3 Chain validation | Must | 3 |
| US-1.4 Balances from history | Must | 3 |
| US-1.5 Halving tokenomics | Should | 2 |
| US-2.1 NFT mint | Must | 5 |
| US-2.2 SSI fingerprint identity | Must | 3 |
| US-2.3 NFT provenance audit | Should | 2 |
| US-3.1 Campaign aggregation | Should | 3 |
| US-3.2 Transparent contribution | Should | 2 |
| US-3.3 Escrow contract | Won't (MVP) | 8 |
| US-4.1 Peer connect + PQC handshake | Must | 8 |
| US-4.2 Longest-valid-chain sync | Must | 5 |
| US-4.3 ZKP-style pseudonymous auth | Must | 3 |
| US-4.4 Ephemeral session encryption | Must | 5 |
| US-5.1 Live ledger visualization | Must | 5 |
| US-5.2 Consensus encyclopedia | Must | 5 |
| US-5.3 PQC education view | Must | 3 |
| US-5.4 Use-case dashboards | Should | 5 |
| US-6.1 PL/EN flag switcher | Must | 3 |

**MVP scope (Must):** 58 points. Definition of Done: acceptance criteria pass, unit tests cover core/crypto/consensus, header comments explain the compliance/security rationale, texts available in EN and PL.
