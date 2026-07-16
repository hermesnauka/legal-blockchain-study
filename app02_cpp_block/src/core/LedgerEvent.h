#pragma once

#include <json/json.h>
#include <string>

namespace legalchain::core {

/// Domain event published by the ledger. The WebSocket broadcaster relays these
/// to the educational frontend at /ws/events so the UI can animate blocks and
/// transactions the moment they happen.
enum class LedgerEventType {
    BLOCK_ADDED,
    TX_ADDED,
    PEER_CONNECTED,
    CHAIN_REPLACED,
    CONSENSUS_CHANGED
};

inline std::string toString(LedgerEventType type) {
    switch (type) {
        case LedgerEventType::BLOCK_ADDED: return "BLOCK_ADDED";
        case LedgerEventType::TX_ADDED: return "TX_ADDED";
        case LedgerEventType::PEER_CONNECTED: return "PEER_CONNECTED";
        case LedgerEventType::CHAIN_REPLACED: return "CHAIN_REPLACED";
        case LedgerEventType::CONSENSUS_CHANGED: return "CONSENSUS_CHANGED";
    }
    return "UNKNOWN";
}

struct LedgerEvent {
    LedgerEventType type;
    Json::Value data;
};

} // namespace legalchain::core
