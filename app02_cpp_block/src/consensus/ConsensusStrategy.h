#pragma once

#include <string>

#include "core/Block.h"
#include "core/BlockCandidate.h"

namespace legalchain::consensus {

/// Strategy pattern for consensus: the ledger core is agnostic about *how* a
/// block earns the right to be appended. Each implementation seals a
/// candidate (producing its proof) and can independently re-validate any
/// sealed block — validation must be a pure function of the block, because
/// remote nodes re-check proofs during P2P sync without trusting the sender.
///
/// This is the "Consensus and Scalability" seam of the architecture: adding
/// DPoS, Proof-of-Reputation, PoH or PoET later means adding one class, not
/// touching the core.
class ConsensusStrategy {
public:
    virtual ~ConsensusStrategy() = default;

    /// Stable identifier used in the API and stored in each block (consensusType).
    virtual std::string name() const = 0;

    /// One-line explanation surfaced by the educational frontend.
    virtual std::string description() const = 0;

    /// Produces the proof for the candidate and returns the sealed, hashed block.
    virtual core::Block seal(const core::BlockCandidate& candidate) const = 0;

    /// Re-verifies the proof of a sealed block (used when auditing chains from peers).
    virtual bool validate(const core::Block& block) const = 0;
};

} // namespace legalchain::consensus
