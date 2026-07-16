#pragma once

#include <drogon/WebSocketController.h>

#include "p2p/P2pService.h"

namespace legalchain::p2p {

/// Thin WebSocket adapter for a node-to-node link at /ws/p2p; all protocol
/// logic lives in P2pService. Every inbound connection here is the
/// non-initiating side of the handshake (it waits for HELLO) — the outbound,
/// initiating side is P2pClientService.
class P2pWebSocketController : public drogon::WebSocketController<P2pWebSocketController, false> {
public:
    explicit P2pWebSocketController(P2pService& p2p) : p2p_(p2p) {}

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& message,
                           const drogon::WebSocketMessageType& type) override {
        if (type == drogon::WebSocketMessageType::Text) {
            p2p_.onMessage(conn, message);
        }
    }

    void handleNewConnection(const drogon::HttpRequestPtr&, const drogon::WebSocketConnectionPtr& conn) override {
        p2p_.registerPeer(conn, /*initiator=*/false, std::nullopt);
    }

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override { p2p_.unregisterPeer(conn); }

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/p2p", drogon::Get);
    WS_PATH_LIST_END

private:
    P2pService& p2p_;
};

} // namespace legalchain::p2p
