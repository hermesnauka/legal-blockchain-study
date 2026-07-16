#include "p2p/EventBroadcaster.h"

#include <algorithm>
#include <trantor/utils/Logger.h>

#include "core/JsonUtil.h"

namespace legalchain::p2p {

EventBroadcaster::EventBroadcaster(core::EventBus& events) : events_(events) {
    events_.subscribe([this](const core::LedgerEvent& event) {
        Json::Value frame(Json::objectValue);
        frame["type"] = core::toString(event.type);
        frame["data"] = event.data;
        std::string json = core::jsonutil::write(frame);

        std::vector<drogon::WebSocketConnectionPtr> alive;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& conn : sessions_) {
                if (conn && !conn->disconnected()) {
                    alive.push_back(conn);
                }
            }
            sessions_ = alive;
        }
        for (auto& conn : alive) {
            conn->send(json);
        }
    });
}

void EventBroadcaster::handleNewConnection(const drogon::HttpRequestPtr&, const drogon::WebSocketConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.push_back(conn);
}

void EventBroadcaster::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(std::remove(sessions_.begin(), sessions_.end(), conn), sessions_.end());
}

} // namespace legalchain::p2p
