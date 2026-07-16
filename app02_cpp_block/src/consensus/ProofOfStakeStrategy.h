#pragma once

#include <utility>
#include <vector>

#include "consensus/ConsensusStrategy.h"

namespace legalchain::consensus {

/// Proof of Stake: the right to seal a block is assigned by stake-weighted
/// lottery instead of computational work.
///
/// The lottery must be deterministic and grinding-resistant so that every
/// node (including a peer receiving the block during sync) selects the same
/// winner: we seed it with the previous block hash — a value the sealer
/// cannot freely re-roll — and walk the cumulative stake distribution.
/// Security intuition: attacking the network requires owning a large share
/// of the very asset the attack would devalue ("skin in the game"), and
/// misbehaviour can be punished by slashing stake. Energy cost is
/// negligible, which is why PoS is the default strategy of this
/// compliance-oriented ledger (ESG reporting is part of regulatory reality).
///
/// MVP simplification: the validator set is a fixed, documented registry;
/// production systems derive it from on-chain staking transactions.
class ProofOfStakeStrategy : public ConsensusStrategy {
public:
    ProofOfStakeStrategy();

    std::string name() const override { return "POS"; }
    std::string description() const override;
    core::Block seal(const core::BlockCandidate& candidate) const override;
    bool validate(const core::Block& block) const override;

private:
    /// Demo validator registry: address -> staked tokens (order matters for determinism).
    std::vector<std::pair<std::string, int64_t>> stakes_;

    std::string selectValidator(const std::string& previousHash) const;
    int64_t stakeOf(const std::string& validator) const;
};

} // namespace legalchain::consensus
