#include "p2p/P2pClientService.h"

#include <chrono>
#include <future>
#include <stdexcept>

namespace legalchain::p2p {

namespace {
struct ParsedWsUrl {
    std::string host;
    uint16_t port;
    std::string path;
};

/// Minimal parser for ws://host:port/path — the only form this educational
/// MVP's P2P connector needs (no wss://, no query strings).
ParsedWsUrl parseWsUrl(const std::string& url) {
    const std::string scheme = "ws://";
    if (url.rfind(scheme, 0) != 0) {
        throw std::invalid_argument("Only ws:// URLs are supported: " + url);
    }
    std::string rest = url.substr(scheme.size());
    auto slashPos = rest.find('/');
    std::string hostPort = slashPos == std::string::npos ? rest : rest.substr(0, slashPos);
    std::string path = slashPos == std::string::npos ? "/" : rest.substr(slashPos);
    auto colonPos = hostPort.find(':');
    std::string host = colonPos == std::string::npos ? hostPort : hostPort.substr(0, colonPos);
    uint16_t port = colonPos == std::string::npos
                        ? 80
                        : static_cast<uint16_t>(std::stoi(hostPort.substr(colonPos + 1)));
    return {host, port, path};
}
} // namespace

void P2pClientService::connect(const std::string& url) {
    ParsedWsUrl parsed = parseWsUrl(url);
    auto client = drogon::WebSocketClient::newWebSocketClient(parsed.host, parsed.port, false);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.push_back(client);
    }

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setPath(parsed.path);

    // Heap-allocated so the callback can safely complete it even if this
    // function has already timed out and returned.
    auto outcome = std::make_shared<std::promise<void>>();
    auto future = outcome->get_future();

    client->setMessageHandler([this](std::string&& message, const drogon::WebSocketClientPtr& wsPtr,
                                      const drogon::WebSocketMessageType& type) {
        if (type == drogon::WebSocketMessageType::Text) {
            p2p_.onMessage(wsPtr->getConnection(), message);
        }
    });
    client->setConnectionClosedHandler(
        [this](const drogon::WebSocketClientPtr& wsPtr) { p2p_.unregisterPeer(wsPtr->getConnection()); });

    client->connectToServer(req, [this, url, outcome](drogon::ReqResult result, const drogon::HttpResponsePtr&,
                                                        const drogon::WebSocketClientPtr& wsPtr) {
        if (result != drogon::ReqResult::Ok) {
            outcome->set_exception(std::make_exception_ptr(std::runtime_error("Cannot connect to peer " + url)));
            return;
        }
        p2p_.registerPeer(wsPtr->getConnection(), /*initiator=*/true, url);
        outcome->set_value();
    });

    if (future.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
        throw std::runtime_error("Cannot connect to peer " + url + ": timeout");
    }
    future.get(); // rethrows the connection failure, if any
}

} // namespace legalchain::p2p
