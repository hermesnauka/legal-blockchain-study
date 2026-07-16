#include "consensus/BftStrategy.h"

#include <sstream>

#include "crypto/HashUtil.h"

namespace legalchain::consensus {

const std::vector<std::string>& BftStrategy::panel() {
    static const std::vector<std::string> PANEL = {"validator-1", "validator-2", "validator-3", "validator-4"};
    return PANEL;
}

std::string BftStrategy::description() const {
    return "Byzantine Fault Tolerance (pBFT): 4 validators, quorum of 3 (tolerates 1 "
           "malicious node); immediate finality, no forks.";
}

core::Block BftStrategy::seal(const core::BlockCandidate& candidate) const {
    std::string subject = candidate.merkleRoot() + "|" + candidate.previousHash();
    std::ostringstream votes;
    int approvals = 0;
    bool first = true;
    for (const auto& validator : panel()) {
        std::string attestation = crypto::HashUtil::sha3(validator + "|" + subject).substr(0, 8);
        if (!first) votes << ",";
        first = false;
        votes << validator << ":APPROVE:" << attestation;
        ++approvals;
        if (approvals == static_cast<int>(panel().size())) break;
    }
    if (approvals < QUORUM) {
        throw std::runtime_error("BFT quorum not reached");
    }
    std::string proof = "quorum=" + std::to_string(approvals) + "/" + std::to_string(panel().size())
                         + ";votes=" + votes.str();
    return candidate.seal(name(), proof, 0);
}

bool BftStrategy::validate(const core::Block& block) const {
    if (!block.hashIsConsistent() || block.proof().compare(0, 7, "quorum=") != 0) {
        return false;
    }
    std::string subject = block.merkleRoot() + "|" + block.previousHash();
    auto votesPos = block.proof().find(";votes=");
    if (votesPos == std::string::npos) return false;
    std::string votesPart = block.proof().substr(votesPos + 7);

    int validVotes = 0;
    std::size_t start = 0;
    while (start <= votesPart.size()) {
        std::size_t comma = votesPart.find(',', start);
        std::string vote = votesPart.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        std::size_t c1 = vote.find(':');
        std::size_t c2 = c1 == std::string::npos ? std::string::npos : vote.find(':', c1 + 1);
        if (c1 != std::string::npos && c2 != std::string::npos) {
            std::string validator = vote.substr(0, c1);
            std::string status = vote.substr(c1 + 1, c2 - c1 - 1);
            std::string attestation = vote.substr(c2 + 1);
            std::string expected = crypto::HashUtil::sha3(validator + "|" + subject).substr(0, 8);
            if (status == "APPROVE" && attestation == expected) {
                ++validVotes;
            }
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return validVotes >= QUORUM;
}

} // namespace legalchain::consensus
