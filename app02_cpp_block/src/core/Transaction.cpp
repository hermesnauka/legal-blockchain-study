#include "core/Transaction.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "core/JsonUtil.h"

namespace legalchain::core {

Transaction::Transaction(std::string id, int64_t timestampMs, std::string sender, std::string recipient,
                          double amount, TransactionType type, std::map<std::string, std::string> payload,
                          std::optional<std::string> senderPublicKey, std::optional<std::string> signature)
    : id_(std::move(id)),
      timestampMs_(timestampMs),
      sender_(std::move(sender)),
      recipient_(std::move(recipient)),
      amount_(amount),
      type_(type),
      payload_(std::move(payload)),
      senderPublicKey_(std::move(senderPublicKey)),
      signature_(std::move(signature)) {}

std::string Transaction::formatAmount(double amount) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(8);
    oss << amount;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        std::size_t last = s.find_last_not_of('0');
        if (s[last] == '.') {
            --last;
        }
        s.erase(last + 1);
    }
    return s;
}

std::string Transaction::contentToSign() const {
    return id_ + "|" + std::to_string(timestampMs_) + "|" + sender_ + "|" + recipient_ + "|"
           + formatAmount(amount_) + "|" + core::toString(type_) + "|"
           + jsonutil::mapToCanonicalString(payload_);
}

Json::Value Transaction::toJson() const {
    Json::Value v(Json::objectValue);
    v["id"] = id_;
    v["timestamp"] = static_cast<Json::Int64>(timestampMs_);
    v["sender"] = sender_;
    v["recipient"] = recipient_;
    v["amount"] = amount_;
    v["type"] = core::toString(type_);
    v["payload"] = jsonutil::mapToJson(payload_);
    v["senderPublicKey"] = senderPublicKey_ ? Json::Value(*senderPublicKey_) : Json::Value();
    v["signature"] = signature_ ? Json::Value(*signature_) : Json::Value();
    return v;
}

Transaction Transaction::fromJson(const Json::Value& json) {
    std::optional<std::string> senderPublicKey;
    if (json.isMember("senderPublicKey") && !json["senderPublicKey"].isNull()) {
        senderPublicKey = json["senderPublicKey"].asString();
    }
    std::optional<std::string> signature;
    if (json.isMember("signature") && !json["signature"].isNull()) {
        signature = json["signature"].asString();
    }
    return Transaction(
        json["id"].asString(),
        json["timestamp"].asInt64(),
        json["sender"].asString(),
        json["recipient"].asString(),
        json["amount"].asDouble(),
        transactionTypeFromString(json["type"].asString()),
        jsonutil::jsonToMap(json["payload"]),
        senderPublicKey,
        signature);
}

} // namespace legalchain::core
