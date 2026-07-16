#include "core/Block.h"

#include "crypto/HashUtil.h"

namespace legalchain::core {

Block::Block(int64_t index, int64_t timestampMs, std::string previousHash, std::string merkleRoot,
             std::vector<Transaction> transactions, std::string validatorId, std::string consensusType,
             std::string proof, int64_t nonce, std::string hash)
    : index_(index),
      timestampMs_(timestampMs),
      previousHash_(std::move(previousHash)),
      merkleRoot_(std::move(merkleRoot)),
      transactions_(std::move(transactions)),
      validatorId_(std::move(validatorId)),
      consensusType_(std::move(consensusType)),
      proof_(std::move(proof)),
      nonce_(nonce),
      hash_(std::move(hash)) {}

std::string Block::computeHash(int64_t index, int64_t timestampMs, const std::string& previousHash,
                                const std::string& merkleRoot, const std::string& validatorId,
                                const std::string& consensusType, const std::string& proof, int64_t nonce) {
    return crypto::HashUtil::sha3(
        std::to_string(index) + "|" + std::to_string(timestampMs) + "|" + previousHash + "|" + merkleRoot
        + "|" + validatorId + "|" + consensusType + "|" + proof + "|" + std::to_string(nonce));
}

std::string Block::merkleRootOf(const std::vector<Transaction>& transactions) {
    std::vector<std::string> leaves;
    leaves.reserve(transactions.size());
    for (const auto& tx : transactions) {
        leaves.push_back(tx.contentToSign());
    }
    return crypto::HashUtil::merkleRoot(leaves);
}

bool Block::hashIsConsistent() const {
    return hash_ == computeHash(index_, timestampMs_, previousHash_, merkleRootOf(transactions_),
                                 validatorId_, consensusType_, proof_, nonce_);
}

bool Block::operator==(const Block& other) const {
    return index_ == other.index_ && hash_ == other.hash_ && previousHash_ == other.previousHash_;
}

Json::Value Block::toJson() const {
    Json::Value v(Json::objectValue);
    v["index"] = static_cast<Json::Int64>(index_);
    v["timestamp"] = static_cast<Json::Int64>(timestampMs_);
    v["previousHash"] = previousHash_;
    v["merkleRoot"] = merkleRoot_;
    Json::Value txs(Json::arrayValue);
    for (const auto& tx : transactions_) {
        txs.append(tx.toJson());
    }
    v["transactions"] = txs;
    v["validatorId"] = validatorId_;
    v["consensusType"] = consensusType_;
    v["proof"] = proof_;
    v["nonce"] = static_cast<Json::Int64>(nonce_);
    v["hash"] = hash_;
    return v;
}

Block Block::fromJson(const Json::Value& json) {
    std::vector<Transaction> transactions;
    for (const auto& txJson : json["transactions"]) {
        transactions.push_back(Transaction::fromJson(txJson));
    }
    return Block(
        json["index"].asInt64(),
        json["timestamp"].asInt64(),
        json["previousHash"].asString(),
        json["merkleRoot"].asString(),
        std::move(transactions),
        json["validatorId"].asString(),
        json["consensusType"].asString(),
        json["proof"].asString(),
        json["nonce"].asInt64(),
        json["hash"].asString());
}

} // namespace legalchain::core
