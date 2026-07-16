#pragma once

#include <cstdint>
#include <json/json.h>
#include <map>
#include <optional>
#include <string>

#include "core/TransactionType.h"

namespace legalchain::core {

/// Immutable ledger transaction. Immutability is a compliance feature: once
/// constructed, an entry cannot be mutated in memory, only superseded on-chain.
///
/// Every non-reward transaction is signed with the sender's ML-DSA key over
/// contentToSign(), and carries the sender's public key so that any node or
/// external auditor can verify authorship offline. The sender "address" is a
/// SHA3-256 fingerprint of that key — pseudonymous, self-sovereign identity.
class Transaction {
public:
    static constexpr const char* SYSTEM = "SYSTEM";

    Transaction() = default;
    Transaction(std::string id, int64_t timestampMs, std::string sender, std::string recipient,
                double amount, TransactionType type, std::map<std::string, std::string> payload,
                std::optional<std::string> senderPublicKey, std::optional<std::string> signature);

    const std::string& id() const { return id_; }
    int64_t timestampMs() const { return timestampMs_; }
    const std::string& sender() const { return sender_; }
    const std::string& recipient() const { return recipient_; }
    double amount() const { return amount_; }
    TransactionType type() const { return type_; }
    const std::map<std::string, std::string>& payload() const { return payload_; }
    const std::optional<std::string>& senderPublicKey() const { return senderPublicKey_; }
    const std::optional<std::string>& signature() const { return signature_; }

    /// Canonical string covered by the ML-DSA signature.
    std::string contentToSign() const;
    bool isReward() const { return type_ == TransactionType::REWARD; }

    Json::Value toJson() const;
    static Transaction fromJson(const Json::Value& json);

    /// Java-BigDecimal-style canonical formatting: no trailing zeros, plain decimal.
    static std::string formatAmount(double amount);

private:
    std::string id_;
    int64_t timestampMs_ = 0;
    std::string sender_;
    std::string recipient_;
    double amount_ = 0.0;
    TransactionType type_ = TransactionType::TRANSFER;
    std::map<std::string, std::string> payload_;
    std::optional<std::string> senderPublicKey_;
    std::optional<std::string> signature_;
};

} // namespace legalchain::core
