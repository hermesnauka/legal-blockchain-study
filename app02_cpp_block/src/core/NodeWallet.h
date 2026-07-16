#pragma once

#include <string>

#include "crypto/PqcSignatureService.h"

namespace legalchain::core {

/// The node's own post-quantum wallet: an ML-DSA-65 key pair generated at startup.
///
/// Self-sovereign identity in practice: the node's address is the SHA3-256
/// fingerprint of its public key. No registration authority issues it, no
/// third party can revoke it, and it reveals nothing about the operator —
/// yet every signature made with it is independently verifiable by anyone.
/// The private key never leaves this process (in the MVP it is intentionally
/// not persisted: restart = new identity).
class NodeWallet {
public:
    explicit NodeWallet(crypto::PqcSignatureService signatures);

    const std::string& address() const { return address_; }
    std::string publicKeyBase64() const;
    std::string sign(const std::string& content) const;
    std::string algorithm() const;

private:
    crypto::PqcSignatureService signatures_;
    crypto::SignatureKeyPair keyPair_;
    std::string address_;
};

} // namespace legalchain::core
