#pragma once

#include <cstdint>
#include <drogon/WebSocketConnection.h>
#include <optional>
#include <string>
#include <vector>

namespace legalchain::p2p {

/// Mutable handshake state for one peer link. A link becomes established only
/// after the ML-DSA-authenticated ML-KEM handshake has produced a one-time
/// AES-256 session key; until then no ledger data flows.
class PeerSession {
public:
    PeerSession(drogon::WebSocketConnectionPtr ws, bool initiator) : ws_(std::move(ws)), initiator_(initiator) {}

    const drogon::WebSocketConnectionPtr& ws() const { return ws_; }
    bool initiator() const { return initiator_; }

    void url(std::string url) { url_ = std::move(url); }
    const std::optional<std::string>& url() const { return url_; }

    void peerNodeId(std::string id) { peerNodeId_ = std::move(id); }
    const std::optional<std::string>& peerNodeId() const { return peerNodeId_; }

    /// Initiator's ephemeral ML-KEM secret key, discarded once the session key is derived.
    void ephemeralKemSecret(std::vector<uint8_t> key) { ephemeralKemSecret_ = std::move(key); }
    const std::vector<uint8_t>& ephemeralKemSecret() const { return ephemeralKemSecret_; }

    void establish(std::vector<uint8_t> key) {
        sessionKey_ = std::move(key);
        ephemeralKemSecret_.clear(); // forward secrecy: ephemeral key is single-use
    }

    bool established() const { return !sessionKey_.empty(); }
    const std::vector<uint8_t>& sessionKey() const { return sessionKey_; }

private:
    drogon::WebSocketConnectionPtr ws_;
    bool initiator_;
    std::optional<std::string> url_;
    std::optional<std::string> peerNodeId_;
    std::vector<uint8_t> ephemeralKemSecret_;
    std::vector<uint8_t> sessionKey_;
};

} // namespace legalchain::p2p
