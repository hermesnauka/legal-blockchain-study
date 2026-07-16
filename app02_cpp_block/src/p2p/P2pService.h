#pragma once

#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/Blockchain.h"
#include "core/EventBus.h"
#include "core/LedgerEvent.h"
#include "core/NodeWallet.h"
#include "crypto/PqcKeyExchangeService.h"
#include "crypto/PqcSignatureService.h"
#include "crypto/SecureChannel.h"
#include "p2p/PeerSession.h"

namespace legalchain::p2p {

/// The P2P ledger-synchronization protocol between two remote nodes, secured
/// end-to-end with post-quantum cryptography.
///
/// Handshake (QKD-inspired, MITM-resistant):
///  1. HELLO (initiator -> responder): the initiator's node id, its ML-DSA
///     public key, a fresh ephemeral ML-KEM public key, and an ML-DSA
///     signature over kemPublicKey|nodeId.
///  2. ENCAPS (responder -> initiator): the responder encapsulates a random
///     32-byte secret against the initiator's KEM key and returns the
///     ciphertext, signed with its own ML-DSA key over ciphertext|nodeId.
///  3. Both sides now hold the same one-time AES-256-GCM session key; every
///     subsequent frame is a SECURE message (authenticated encryption).
///
/// Why a man-in-the-middle fails: both handshake messages are signed with
/// ML-DSA, and each side checks that the claimed node id equals the
/// SHA3-256 fingerprint of the signing key. An attacker who swaps the KEM
/// key or the ciphertext must forge an ML-DSA signature — believed
/// infeasible even for a quantum adversary (lattice hardness, NIST FIPS
/// 204). Tampering with SECURE frames fails the GCM authentication tag
/// instead.
///
/// Trust without revealing identity (ZKP-style pseudonymity): the handshake
/// contains no names, certificates, or IP-bound identities. Each party
/// proves exactly one statement — "I control the private key behind this
/// fingerprint" — by producing a valid signature, and nothing else. Even the
/// adopted ledger data is never trusted on channel security alone —
/// Blockchain::replaceChain re-audits every block, signature and consensus
/// proof locally before accepting a peer's chain.
class P2pService {
public:
    P2pService(core::Blockchain& blockchain, core::NodeWallet& wallet, crypto::PqcSignatureService signatures,
               crypto::PqcKeyExchangeService keyExchange, crypto::SecureChannel channel, core::EventBus& events);

    void registerPeer(const drogon::WebSocketConnectionPtr& ws, bool initiator, std::optional<std::string> url);
    void unregisterPeer(const drogon::WebSocketConnectionPtr& ws);
    void onMessage(const drogon::WebSocketConnectionPtr& ws, const std::string& text);

    Json::Value connectedPeers() const;

    /// Asks every established peer for its chain; longest valid chain wins locally.
    int syncWithPeers();

private:
    core::Blockchain& blockchain_;
    core::NodeWallet& wallet_;
    crypto::PqcSignatureService signatures_;
    crypto::PqcKeyExchangeService keyExchange_;
    crypto::SecureChannel channel_;
    core::EventBus& events_;

    mutable std::mutex mutex_;
    std::map<drogon::WebSocketConnectionPtr, std::shared_ptr<PeerSession>> peers_;

    void sendHello(PeerSession& peer);
    void onHello(PeerSession& peer, const Json::Value& msg);
    void onEncaps(PeerSession& peer, const Json::Value& msg);
    void onSecure(PeerSession& peer, const Json::Value& msg);
    void requireAuthentic(const std::string& nodeId, const std::string& dsaPub, const std::string& signedContent,
                           const std::string& signature) const;

    void sendSecure(PeerSession& peer, const Json::Value& inner);
    static void send(PeerSession& peer, const Json::Value& message);

    std::shared_ptr<PeerSession> find(const drogon::WebSocketConnectionPtr& ws) const;
};

} // namespace legalchain::p2p
