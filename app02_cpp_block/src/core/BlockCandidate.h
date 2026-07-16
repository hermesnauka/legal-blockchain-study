#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Block.h"
#include "core/Transaction.h"

namespace legalchain::core {

/// An unsealed block handed to a consensus::ConsensusStrategy, which produces
/// the proof/nonce and the final Block. Separating the candidate from the
/// sealed block is what lets consensus algorithms be swapped (Strategy
/// pattern) without touching the ledger core.
class BlockCandidate {
public:
    BlockCandidate(int64_t index, int64_t timestampMs, std::string previousHash,
                    std::vector<Transaction> transactions, std::string validatorId)
        : index_(index),
          timestampMs_(timestampMs),
          previousHash_(std::move(previousHash)),
          transactions_(std::move(transactions)),
          validatorId_(std::move(validatorId)) {}

    int64_t index() const { return index_; }
    int64_t timestampMs() const { return timestampMs_; }
    const std::string& previousHash() const { return previousHash_; }
    const std::vector<Transaction>& transactions() const { return transactions_; }
    const std::string& validatorId() const { return validatorId_; }

    std::string merkleRoot() const { return Block::merkleRootOf(transactions_); }

    /// Convenience for strategies: seal this candidate with the given proof and nonce.
    Block seal(const std::string& consensusType, const std::string& proof, int64_t nonce) const {
        std::string root = merkleRoot();
        std::string hash = Block::computeHash(index_, timestampMs_, previousHash_, root, validatorId_,
                                               consensusType, proof, nonce);
        return Block(index_, timestampMs_, previousHash_, root, transactions_, validatorId_,
                     consensusType, proof, nonce, hash);
    }

private:
    int64_t index_;
    int64_t timestampMs_;
    std::string previousHash_;
    std::vector<Transaction> transactions_;
    std::string validatorId_;
};

} // namespace legalchain::core
