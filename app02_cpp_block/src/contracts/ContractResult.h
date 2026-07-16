#pragma once

#include <map>
#include <optional>
#include <string>

#include "core/JsonUtil.h"

namespace legalchain::contracts {

/// Outcome of a smart-contract execution: the on-chain record (payload) plus
/// a human-readable message for the UI.
struct ContractResult {
    std::string contractId;
    bool accepted = false;
    std::string message;
    /// The exact key/value data recorded in the ledger transaction.
    std::map<std::string, std::string> record;
    /// Id of the ledger transaction recording this execution (set by the engine).
    std::optional<std::string> txId;

    ContractResult withTxId(const std::string& id) const {
        ContractResult copy = *this;
        copy.txId = id;
        return copy;
    }

    Json::Value toJson() const {
        Json::Value v(Json::objectValue);
        v["contractId"] = contractId;
        v["accepted"] = accepted;
        v["message"] = message;
        v["record"] = core::jsonutil::mapToJson(record);
        v["txId"] = txId ? Json::Value(*txId) : Json::Value();
        return v;
    }
};

} // namespace legalchain::contracts
