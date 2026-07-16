#pragma once

#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "consensus/ConsensusEngine.h"
#include "core/Block.h"
#include "core/EventBus.h"
#include "core/TokenomicsService.h"
#include "core/Transaction.h"
#include "crypto/PqcSignatureService.h"

namespace legalchain::core {

/// The general ledger: an append-only, hash-chained list of blocks plus the
/// pool of pending (signed, not yet sealed) transactions.
///
/// This is a real ledger, not a mock: balances are never stored — they are
/// derived by replaying every transaction from genesis (balances()),
/// signatures are verified with ML-DSA before a transaction is accepted, and
/// isValidChain() re-audits hashes, links, Merkle roots, signatures and
/// consensus proofs of the entire history. That auditability-from-first-
/// principles is the property that makes a blockchain acceptable as a
/// compliant book of record.
///
/// Thread-safety: mutating operations hold chainMutex_; reads return
/// defensive copies — the C++ analogue of the Java port's
/// `synchronized(chain)` blocks.
class Blockchain {
public:
    Blockchain(consensus::ConsensusEngine& consensusEngine, TokenomicsService tokenomics,
               crypto::PqcSignatureService signatures, EventBus& events);

    /// Accepts a transaction into the pending pool after verifying its
    /// ML-DSA signature, that the claimed sender address really is the
    /// fingerprint of the signing key, and that the sender can actually
    /// cover the amount. Throws std::invalid_argument on any violation.
    Transaction submit(const Transaction& tx);

    /// Seals the pending transactions into a new block via the active
    /// consensus strategy and credits the validator with the tokenomics reward.
    Block minePendingTransactions(const std::string& validatorAddress);

    /// Full audit of a chain: genesis identity, hash consistency, previous-hash
    /// links, transaction signatures, and each block's own consensus proof.
    bool isValidChain(const std::vector<Block>& candidate) const;
    bool isValid() const;

    /// Longest-valid-chain rule used by P2P sync: adopt a peer's chain only
    /// if it is strictly longer *and* passes the full local audit.
    bool replaceChain(const std::vector<Block>& candidate);

    /// Balances derived by replaying the full ledger — the "general ledger" property.
    std::map<std::string, double> balances() const;

    std::vector<Block> blocks() const;
    std::vector<Transaction> pendingTransactions() const;
    int length() const;

    static const Block& genesis();

private:
    mutable std::mutex chainMutex_;
    std::vector<Block> chain_;
    std::vector<Transaction> pending_;

    consensus::ConsensusEngine& consensusEngine_;
    TokenomicsService tokenomics_;
    crypto::PqcSignatureService signatures_;
    EventBus& events_;

    double pendingOutgoingOf(const std::string& sender) const;
};

} // namespace legalchain::core
