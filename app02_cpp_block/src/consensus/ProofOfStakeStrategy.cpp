#include "consensus/ProofOfStakeStrategy.h"

#include <stdexcept>

#include "crypto/HashUtil.h"

namespace legalchain::consensus {

ProofOfStakeStrategy::ProofOfStakeStrategy() {
    stakes_.emplace_back("validator-alpha", 60);
    stakes_.emplace_back("validator-beta", 30);
    stakes_.emplace_back("validator-gamma", 10);
}

std::string ProofOfStakeStrategy::description() const {
    return "Proof of Stake: deterministic stake-weighted lottery seeded by the previous "
           "block hash; negligible energy use, security from economic stake.";
}

int64_t ProofOfStakeStrategy::stakeOf(const std::string& validator) const {
    for (const auto& [name, stake] : stakes_) {
        if (name == validator) return stake;
    }
    return 0;
}

std::string ProofOfStakeStrategy::selectValidator(const std::string& previousHash) const {
    int64_t totalStake = 0;
    for (const auto& [name, stake] : stakes_) totalStake += stake;

    // First 12 hex chars of SHA3(previousHash) -> uniform draw in [0, totalStake).
    std::string digest = crypto::HashUtil::sha3(previousHash).substr(0, 12);
    uint64_t drawRaw = std::stoull(digest, nullptr, 16);
    int64_t draw = static_cast<int64_t>(drawRaw % static_cast<uint64_t>(totalStake));

    int64_t cumulative = 0;
    for (const auto& [name, stake] : stakes_) {
        cumulative += stake;
        if (draw < cumulative) {
            return name;
        }
    }
    throw std::runtime_error("Stake distribution exhausted — unreachable");
}

core::Block ProofOfStakeStrategy::seal(const core::BlockCandidate& candidate) const {
    std::string winner = selectValidator(candidate.previousHash());
    std::string proof = "stake-winner=" + winner + ";stake=" + std::to_string(stakeOf(winner))
                         + ";seed=" + candidate.previousHash().substr(0, 12);
    return candidate.seal(name(), proof, 0);
}

bool ProofOfStakeStrategy::validate(const core::Block& block) const {
    std::string expectedWinner = selectValidator(block.previousHash());
    std::string expectedPrefix = "stake-winner=" + expectedWinner + ";";
    return block.proof().compare(0, expectedPrefix.size(), expectedPrefix) == 0 && block.hashIsConsistent();
}

} // namespace legalchain::consensus
