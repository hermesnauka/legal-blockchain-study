#pragma once

#include <stdexcept>
#include <string>

namespace legalchain::core {

/// Ledger entry categories; contract types make sector use-cases auditable on-chain.
enum class TransactionType {
    TRANSFER,
    REWARD,
    NFT_MINT,
    CONTRACT_MEDICAL,
    CONTRACT_AGRI
};

inline std::string toString(TransactionType type) {
    switch (type) {
        case TransactionType::TRANSFER: return "TRANSFER";
        case TransactionType::REWARD: return "REWARD";
        case TransactionType::NFT_MINT: return "NFT_MINT";
        case TransactionType::CONTRACT_MEDICAL: return "CONTRACT_MEDICAL";
        case TransactionType::CONTRACT_AGRI: return "CONTRACT_AGRI";
    }
    throw std::invalid_argument("Unknown TransactionType");
}

inline TransactionType transactionTypeFromString(const std::string& s) {
    if (s == "TRANSFER") return TransactionType::TRANSFER;
    if (s == "REWARD") return TransactionType::REWARD;
    if (s == "NFT_MINT") return TransactionType::NFT_MINT;
    if (s == "CONTRACT_MEDICAL") return TransactionType::CONTRACT_MEDICAL;
    if (s == "CONTRACT_AGRI") return TransactionType::CONTRACT_AGRI;
    throw std::invalid_argument("Unknown TransactionType: " + s);
}

} // namespace legalchain::core
