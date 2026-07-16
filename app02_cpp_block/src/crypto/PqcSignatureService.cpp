#include "crypto/PqcSignatureService.h"

#include <oqs/oqs.h>
#include <memory>
#include <stdexcept>

#include "crypto/Base64.h"
#include "crypto/HashUtil.h"

namespace legalchain::crypto {

namespace {
struct SigDeleter {
    void operator()(OQS_SIG* sig) const { OQS_SIG_free(sig); }
};
using SigPtr = std::unique_ptr<OQS_SIG, SigDeleter>;

SigPtr newSig() {
    SigPtr sig(OQS_SIG_new(OQS_SIG_alg_ml_dsa_65));
    if (!sig) {
        throw std::runtime_error("liboqs: ML-DSA-65 not available in this build");
    }
    return sig;
}
} // namespace

std::string PqcSignatureService::algorithmName() {
    return "ML-DSA-65 (FIPS 204 / CRYSTALS-Dilithium)";
}

SignatureKeyPair PqcSignatureService::generateKeyPair() const {
    auto sig = newSig();
    SignatureKeyPair pair;
    pair.publicKey.resize(sig->length_public_key);
    pair.secretKey.resize(sig->length_secret_key);
    if (OQS_SIG_keypair(sig.get(), pair.publicKey.data(), pair.secretKey.data()) != OQS_SUCCESS) {
        throw std::runtime_error("Cannot generate ML-DSA key pair");
    }
    return pair;
}

std::string PqcSignatureService::sign(const std::string& content, const std::vector<uint8_t>& secretKey) const {
    auto sig = newSig();
    std::vector<uint8_t> signature(sig->length_signature);
    std::size_t signatureLen = 0;
    OQS_STATUS rc = OQS_SIG_sign(sig.get(), signature.data(), &signatureLen,
                                  reinterpret_cast<const uint8_t*>(content.data()), content.size(),
                                  secretKey.data());
    if (rc != OQS_SUCCESS) {
        throw std::runtime_error("ML-DSA signing failed");
    }
    signature.resize(signatureLen);
    return Base64::encode(signature);
}

bool PqcSignatureService::verify(const std::string& content, const std::string& signatureBase64,
                                  const std::string& publicKeyBase64) const {
    try {
        auto sig = newSig();
        auto publicKey = Base64::decode(publicKeyBase64);
        auto signature = Base64::decode(signatureBase64);
        if (publicKey.size() != sig->length_public_key) {
            return false;
        }
        OQS_STATUS rc = OQS_SIG_verify(sig.get(), reinterpret_cast<const uint8_t*>(content.data()),
                                        content.size(), signature.data(), signature.size(), publicKey.data());
        return rc == OQS_SUCCESS;
    } catch (...) {
        // A malformed key or signature is simply an invalid signature, not a server error.
        return false;
    }
}

std::vector<uint8_t> PqcSignatureService::decodePublicKey(const std::string& publicKeyBase64) const {
    return Base64::decode(publicKeyBase64);
}

std::string PqcSignatureService::encodePublicKey(const std::vector<uint8_t>& publicKey) {
    return Base64::encode(publicKey);
}

std::string PqcSignatureService::fingerprint(const std::vector<uint8_t>& publicKey) {
    return "lc" + HashUtil::sha3(encodePublicKey(publicKey)).substr(0, 40);
}

} // namespace legalchain::crypto
