#pragma once

#include <cstdint>
#include <json/json.h>
#include <string>
#include <vector>

#include "core/Transaction.h"

namespace legalchain::core {

/// Immutable block of the general ledger.
///
/// The block hash covers the index, timestamp, previous hash, Merkle root,
/// validator, consensus proof and nonce. Because previousHash of block N+1 is
/// the hash of block N, retroactively editing *any* historical transaction
/// invalidates every later block — this hash chaining is the core
/// tamper-evidence property that makes a blockchain acceptable as a
/// compliant, auditable registry.
class Block {
public:
    Block() = default;
    Block(int64_t index, int64_t timestampMs, std::string previousHash, std::string merkleRoot,
          std::vector<Transaction> transactions, std::string validatorId, std::string consensusType,
          std::string proof, int64_t nonce, std::string hash);

    int64_t index() const { return index_; }
    int64_t timestampMs() const { return timestampMs_; }
    const std::string& previousHash() const { return previousHash_; }
    const std::string& merkleRoot() const { return merkleRoot_; }
    const std::vector<Transaction>& transactions() const { return transactions_; }
    const std::string& validatorId() const { return validatorId_; }
    const std::string& consensusType() const { return consensusType_; }
    const std::string& proof() const { return proof_; }
    int64_t nonce() const { return nonce_; }
    const std::string& hash() const { return hash_; }

    /// Recomputes the header hash; used both when sealing and when auditing the chain.
    static std::string computeHash(int64_t index, int64_t timestampMs, const std::string& previousHash,
                                    const std::string& merkleRoot, const std::string& validatorId,
                                    const std::string& consensusType, const std::string& proof, int64_t nonce);

    static std::string merkleRootOf(const std::vector<Transaction>& transactions);

    /// True if the stored hash matches a recomputation of the header — tamper check.
    bool hashIsConsistent() const;

    bool operator==(const Block& other) const;

    Json::Value toJson() const;
    static Block fromJson(const Json::Value& json);

private:
    int64_t index_ = 0;
    int64_t timestampMs_ = 0;
    std::string previousHash_;
    std::string merkleRoot_;
    std::vector<Transaction> transactions_;
    std::string validatorId_;
    std::string consensusType_;
    std::string proof_;
    int64_t nonce_ = 0;
    std::string hash_;
};

} // namespace legalchain::core
