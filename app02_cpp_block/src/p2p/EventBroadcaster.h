#pragma once

#include <drogon/WebSocketController.h>
#include <mutex>
#include <vector>

#include "core/EventBus.h"

namespace legalchain::p2p {

/// Frontend live feed at /ws/events: relays every core::LedgerEvent to all
/// connected browsers as {"type": ..., "data": ...} so the educational UI can
/// animate blocks, transactions and peer connections in real time. This
/// channel carries only data that is public on the ledger anyway, so unlike
/// /ws/p2p it is not encrypted at the application layer.
class EventBroadcaster : public drogon::WebSocketController<EventBroadcaster, false> {
public:
    explicit EventBroadcaster(core::EventBus& events);

    void handleNewMessage(const drogon::WebSocketConnectionPtr&, std::string&&,
                           const drogon::WebSocketMessageType&) override {}
    void handleNewConnection(const drogon::HttpRequestPtr&, const drogon::WebSocketConnectionPtr& conn) override;
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/events", drogon::Get);
    WS_PATH_LIST_END

private:
    core::EventBus& events_;
    std::mutex mutex_;
    std::vector<drogon::WebSocketConnectionPtr> sessions_;
};

} // namespace legalchain::p2p
