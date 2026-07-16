#pragma once

#include "consensus/ConsensusStrategy.h"

namespace legalchain::consensus {

/// Proof of Work: the sealer increments a nonce until the block hash falls
/// below a difficulty target (here: a required prefix of zero hex digits).
///
/// Security intuition: rewriting history requires redoing the accumulated
/// work of every later block faster than the honest network extends it —
/// economically infeasible past a few confirmations. The proof is trivially
/// verifiable (one hash) but expensive to produce, an asymmetry that needs no
/// identities at all. Trade-off: enormous energy cost, which is why this MVP
/// defaults to PoS and keeps difficulty low (4 hex zeros, ~65k hashes) for
/// classroom responsiveness.
class ProofOfWorkStrategy : public ConsensusStrategy {
public:
    static constexpr const char* DIFFICULTY_PREFIX = "0000";

    std::string name() const override { return "POW"; }
    std::string description() const override;
    core::Block seal(const core::BlockCandidate& candidate) const override;
    bool validate(const core::Block& block) const override;
};

} // namespace legalchain::consensus
