#pragma once

#include <vector>

#include "consensus/ConsensusStrategy.h"

namespace legalchain::consensus {

/// Byzantine Fault Tolerance (pBFT-style, simulated with an in-process
/// validator panel).
///
/// The classical result: with n = 3f + 1 validators, agreement is guaranteed
/// even if f of them are arbitrarily malicious ("Byzantine"), because any two
/// quorums of 2f + 1 votes intersect in at least one honest validator. Here
/// n = 4, f = 1: a block is sealed only when at least 3 of the 4 panel
/// members approve the candidate's Merkle root. BFT gives immediate finality
/// (no probabilistic confirmations, no forks), which is precisely the
/// property permissioned, regulated ledgers need — a court or auditor can
/// treat an appended entry as final.
///
/// Votes are simulated deterministically (each validator independently
/// recomputes and signs off on the candidate hash), so peers can re-validate
/// the recorded quorum. In a production network each vote would be an
/// ML-DSA signature from a distinct node.
class BftStrategy : public ConsensusStrategy {
public:
    std::string name() const override { return "BFT"; }
    std::string description() const override;
    core::Block seal(const core::BlockCandidate& candidate) const override;
    bool validate(const core::Block& block) const override;

private:
    static const std::vector<std::string>& panel();
    static constexpr int QUORUM = 3; // 2f + 1 with f = 1
};

} // namespace legalchain::consensus
