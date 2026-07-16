#include "p2p/P2pService.h"

#include <trantor/utils/Logger.h>

#include "core/JsonUtil.h"

namespace legalchain::p2p {

P2pService::P2pService(core::Blockchain& blockchain, core::NodeWallet& wallet,
                        crypto::PqcSignatureService signatures, crypto::PqcKeyExchangeService keyExchange,
                        crypto::SecureChannel channel, core::EventBus& events)
    : blockchain_(blockchain),
      wallet_(wallet),
      signatures_(std::move(signatures)),
      keyExchange_(std::move(keyExchange)),
      channel_(std::move(channel)),
      events_(events) {
    events_.subscribe([this](const core::LedgerEvent& event) {
        if (event.type != core::LedgerEventType::BLOCK_ADDED) {
            return;
        }
        // Gossip: whenever this node seals a block, tell peers so they can
        // pull the longer chain.
        std::vector<std::shared_ptr<PeerSession>> established;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [conn, peer] : peers_) {
                if (peer->established()) established.push_back(peer);
            }
        }
        for (auto& peer : established) {
            Json::Value announce(Json::objectValue);
            announce["type"] = "ANNOUNCE";
            announce["length"] = blockchain_.length();
            sendSecure(*peer, announce);
        }
    });
}

std::shared_ptr<PeerSession> P2pService::find(const drogon::WebSocketConnectionPtr& ws) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(ws);
    return it == peers_.end() ? nullptr : it->second;
}

void P2pService::registerPeer(const drogon::WebSocketConnectionPtr& ws, bool initiator,
                               std::optional<std::string> url) {
    auto peer = std::make_shared<PeerSession>(ws, initiator);
    if (url) peer->url(*url);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        peers_[ws] = peer;
    }
    if (initiator) {
        sendHello(*peer);
    }
}

void P2pService::unregisterPeer(const drogon::WebSocketConnectionPtr& ws) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.erase(ws);
}

Json::Value P2pService::connectedPeers() const {
    Json::Value result(Json::arrayValue);
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [conn, peer] : peers_) {
        if (!peer->established()) continue;
        Json::Value entry(Json::objectValue);
        entry["nodeId"] = peer->peerNodeId().value_or("?");
        entry["url"] = peer->url().value_or("(inbound)");
        result.append(entry);
    }
    return result;
}

void P2pService::sendHello(PeerSession& peer) {
    // Fresh ephemeral KEM key pair per connection: compromise of one session
    // never affects another (QKD-inspired key freshness).
    auto kemPair = keyExchange_.generateKeyPair();
    peer.ephemeralKemSecret(kemPair.secretKey);
    std::string kemPub = crypto::PqcKeyExchangeService::encodePublicKey(kemPair.publicKey);

    Json::Value hello(Json::objectValue);
    hello["type"] = "HELLO";
    hello["nodeId"] = wallet_.address();
    hello["dsaPub"] = wallet_.publicKeyBase64();
    hello["kemPub"] = kemPub;
    // Signature binds the ephemeral KEM key to this node's ML-DSA identity.
    hello["signature"] = wallet_.sign(kemPub + "|" + wallet_.address());
    send(peer, hello);
}

void P2pService::onMessage(const drogon::WebSocketConnectionPtr& ws, const std::string& text) {
    auto peer = find(ws);
    if (!peer) return;
    try {
        Json::Value msg = core::jsonutil::parse(text);
        std::string type = msg.get("type", "").asString();
        if (type == "HELLO") {
            onHello(*peer, msg);
        } else if (type == "ENCAPS") {
            onEncaps(*peer, msg);
        } else if (type == "SECURE") {
            onSecure(*peer, msg);
        } else {
            LOG_WARN << "Unknown P2P message type from peer";
        }
    } catch (const std::exception& e) {
        LOG_WARN << "Rejected P2P message: " << e.what();
    }
}

void P2pService::requireAuthentic(const std::string& nodeId, const std::string& dsaPub,
                                   const std::string& signedContent, const std::string& signature) const {
    if (!signatures_.verify(signedContent, signature, dsaPub)) {
        throw std::runtime_error("Handshake rejected: invalid ML-DSA signature");
    }
    auto publicKeyBytes = signatures_.decodePublicKey(dsaPub);
    if (crypto::PqcSignatureService::fingerprint(publicKeyBytes) != nodeId) {
        throw std::runtime_error("Handshake rejected: node id is not the key fingerprint");
    }
}

void P2pService::onHello(PeerSession& peer, const Json::Value& msg) {
    std::string nodeId = msg["nodeId"].asString();
    std::string dsaPub = msg["dsaPub"].asString();
    std::string kemPub = msg["kemPub"].asString();
    std::string signature = msg["signature"].asString();

    requireAuthentic(nodeId, dsaPub, kemPub + "|" + nodeId, signature);

    // Derive the one-time session key and send the signed ciphertext back.
    auto encapsulation = keyExchange_.encapsulate(kemPub);
    peer.peerNodeId(nodeId);
    peer.establish(encapsulation.sessionKey);

    Json::Value encaps(Json::objectValue);
    encaps["type"] = "ENCAPS";
    encaps["nodeId"] = wallet_.address();
    encaps["dsaPub"] = wallet_.publicKeyBase64();
    encaps["ciphertext"] = encapsulation.ciphertextBase64;
    encaps["signature"] = wallet_.sign(encapsulation.ciphertextBase64 + "|" + wallet_.address());
    send(peer, encaps);

    Json::Value data(Json::objectValue);
    data["nodeId"] = nodeId;
    data["direction"] = "inbound";
    events_.publish(core::LedgerEvent{core::LedgerEventType::PEER_CONNECTED, data});
    LOG_INFO << "PQC channel established with inbound peer " << nodeId;
}

void P2pService::onEncaps(PeerSession& peer, const Json::Value& msg) {
    std::string nodeId = msg["nodeId"].asString();
    std::string dsaPub = msg["dsaPub"].asString();
    std::string ciphertext = msg["ciphertext"].asString();
    std::string signature = msg["signature"].asString();

    requireAuthentic(nodeId, dsaPub, ciphertext + "|" + nodeId, signature);

    peer.peerNodeId(nodeId);
    peer.establish(keyExchange_.decapsulate(ciphertext, peer.ephemeralKemSecret()));

    Json::Value data(Json::objectValue);
    data["nodeId"] = nodeId;
    data["direction"] = "outbound";
    events_.publish(core::LedgerEvent{core::LedgerEventType::PEER_CONNECTED, data});
    LOG_INFO << "PQC channel established with outbound peer " << nodeId;

    // First act on a fresh secure channel: ask for the peer's ledger.
    Json::Value request(Json::objectValue);
    request["type"] = "CHAIN_REQUEST";
    sendSecure(peer, request);
}

void P2pService::onSecure(PeerSession& peer, const Json::Value& msg) {
    if (!peer.established()) {
        throw std::runtime_error("SECURE frame before handshake completion");
    }
    // GCM decryption throws on any tampering — integrity and confidentiality in one step.
    std::string plaintext = channel_.decrypt(msg["payload"].asString(), peer.sessionKey());
    Json::Value inner = core::jsonutil::parse(plaintext);
    std::string type = inner.get("type", "").asString();

    if (type == "CHAIN_REQUEST") {
        Json::Value response(Json::objectValue);
        response["type"] = "CHAIN_RESPONSE";
        Json::Value blocksJson(Json::arrayValue);
        for (const auto& block : blockchain_.blocks()) {
            blocksJson.append(block.toJson());
        }
        response["blocks"] = blocksJson;
        sendSecure(peer, response);
    } else if (type == "CHAIN_RESPONSE") {
        std::vector<core::Block> remoteChain;
        for (const auto& blockJson : inner["blocks"]) {
            remoteChain.push_back(core::Block::fromJson(blockJson));
        }
        bool adopted = blockchain_.replaceChain(remoteChain);
        LOG_INFO << "Chain from " << peer.peerNodeId().value_or("?") << ": length=" << remoteChain.size()
                 << " adopted=" << adopted;
    } else if (type == "ANNOUNCE") {
        int64_t remoteLength = inner["length"].asInt64();
        if (remoteLength > blockchain_.length()) {
            Json::Value request(Json::objectValue);
            request["type"] = "CHAIN_REQUEST";
            sendSecure(peer, request);
        }
    } else {
        LOG_WARN << "Unknown secure message from " << peer.peerNodeId().value_or("?");
    }
}

int P2pService::syncWithPeers() {
    std::vector<std::shared_ptr<PeerSession>> established;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [conn, peer] : peers_) {
            if (peer->established()) established.push_back(peer);
        }
    }
    for (auto& peer : established) {
        Json::Value request(Json::objectValue);
        request["type"] = "CHAIN_REQUEST";
        sendSecure(*peer, request);
    }
    return static_cast<int>(established.size());
}

void P2pService::sendSecure(PeerSession& peer, const Json::Value& inner) {
    std::string plaintext = core::jsonutil::write(inner);
    Json::Value secure(Json::objectValue);
    secure["type"] = "SECURE";
    secure["payload"] = channel_.encrypt(plaintext, peer.sessionKey());
    send(peer, secure);
}

void P2pService::send(PeerSession& peer, const Json::Value& message) {
    if (peer.ws() && !peer.ws()->disconnected()) {
        peer.ws()->send(core::jsonutil::write(message));
    }
}

} // namespace legalchain::p2p
