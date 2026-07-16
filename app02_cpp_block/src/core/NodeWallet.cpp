#include "core/NodeWallet.h"

namespace legalchain::core {

NodeWallet::NodeWallet(crypto::PqcSignatureService signatures)
    : signatures_(std::move(signatures)),
      keyPair_(signatures_.generateKeyPair()),
      address_(crypto::PqcSignatureService::fingerprint(keyPair_.publicKey)) {}

std::string NodeWallet::publicKeyBase64() const {
    return crypto::PqcSignatureService::encodePublicKey(keyPair_.publicKey);
}

std::string NodeWallet::sign(const std::string& content) const {
    return signatures_.sign(content, keyPair_.secretKey);
}

std::string NodeWallet::algorithm() const {
    return crypto::PqcSignatureService::algorithmName();
}

} // namespace legalchain::core
