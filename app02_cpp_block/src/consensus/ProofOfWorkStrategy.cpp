#include "consensus/ProofOfWorkStrategy.h"

#include <cstring>

namespace legalchain::consensus {

std::string ProofOfWorkStrategy::description() const {
    return std::string("Proof of Work: nonce search until the SHA3-256 block hash starts with ")
           + DIFFICULTY_PREFIX + "; costly to produce, one hash to verify.";
}

core::Block ProofOfWorkStrategy::seal(const core::BlockCandidate& candidate) const {
    int64_t nonce = 0;
    std::size_t prefixLen = std::strlen(DIFFICULTY_PREFIX);
    while (true) {
        core::Block block = candidate.seal(name(), std::string("difficulty=") + std::to_string(prefixLen), nonce);
        if (block.hash().compare(0, prefixLen, DIFFICULTY_PREFIX) == 0) {
            return block;
        }
        ++nonce;
    }
}

bool ProofOfWorkStrategy::validate(const core::Block& block) const {
    std::size_t prefixLen = std::strlen(DIFFICULTY_PREFIX);
    return block.hash().compare(0, prefixLen, DIFFICULTY_PREFIX) == 0 && block.hashIsConsistent();
}

} // namespace legalchain::consensus
