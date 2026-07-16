#pragma once

#include <drogon/WebSocketClient.h>
#include <mutex>
#include <string>
#include <vector>

#include "p2p/P2pService.h"

namespace legalchain::p2p {

/// Outbound side of the P2P mesh: connects this node to a remote peer's
/// /ws/p2p endpoint. The connection attempt runs on Drogon's event loop; the
/// moment the socket opens, this service registers the link with P2pService
/// as the initiator, which immediately starts the PQC handshake by sending
/// HELLO.
class P2pClientService {
public:
    explicit P2pClientService(P2pService& p2p) : p2p_(p2p) {}

    /// Connects to a peer, e.g. ws://localhost:8091/ws/p2p. Blocks (up to 10s)
    /// until the WebSocket handshake completes; throws std::runtime_error on
    /// failure or timeout.
    void connect(const std::string& url);

private:
    P2pService& p2p_;
    std::mutex mutex_;
    std::vector<drogon::WebSocketClientPtr> clients_; // kept alive for the process lifetime
};

} // namespace legalchain::p2p
