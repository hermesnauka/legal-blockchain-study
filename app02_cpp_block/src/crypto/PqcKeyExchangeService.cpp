#include "crypto/PqcKeyExchangeService.h"

#include <oqs/oqs.h>
#include <memory>
#include <stdexcept>

#include "crypto/Base64.h"

namespace legalchain::crypto {

namespace {
struct KemDeleter {
    void operator()(OQS_KEM* kem) const { OQS_KEM_free(kem); }
};
using KemPtr = std::unique_ptr<OQS_KEM, KemDeleter>;

KemPtr newKem() {
    KemPtr kem(OQS_KEM_new(OQS_KEM_alg_ml_kem_768));
    if (!kem) {
        throw std::runtime_error("liboqs: ML-KEM-768 not available in this build");
    }
    return kem;
}
} // namespace

std::string PqcKeyExchangeService::algorithmName() {
    return "ML-KEM-768 (FIPS 203 / CRYSTALS-Kyber)";
}

KemKeyPair PqcKeyExchangeService::generateKeyPair() const {
    auto kem = newKem();
    KemKeyPair pair;
    pair.publicKey.resize(kem->length_public_key);
    pair.secretKey.resize(kem->length_secret_key);
    if (OQS_KEM_keypair(kem.get(), pair.publicKey.data(), pair.secretKey.data()) != OQS_SUCCESS) {
        throw std::runtime_error("Cannot generate ML-KEM key pair");
    }
    return pair;
}

Encapsulation PqcKeyExchangeService::encapsulate(const std::string& receiverPublicKeyBase64) const {
    auto kem = newKem();
    auto receiverKey = Base64::decode(receiverPublicKeyBase64);
    if (receiverKey.size() != kem->length_public_key) {
        throw std::runtime_error("ML-KEM encapsulation failed: malformed public key");
    }
    Encapsulation result;
    std::vector<uint8_t> ciphertext(kem->length_ciphertext);
    result.sessionKey.resize(kem->length_shared_secret);
    if (OQS_KEM_encaps(kem.get(), ciphertext.data(), result.sessionKey.data(), receiverKey.data())
        != OQS_SUCCESS) {
        throw std::runtime_error("ML-KEM encapsulation failed");
    }
    result.ciphertextBase64 = Base64::encode(ciphertext);
    return result;
}

std::vector<uint8_t> PqcKeyExchangeService::decapsulate(const std::string& ciphertextBase64,
                                                          const std::vector<uint8_t>& secretKey) const {
    auto kem = newKem();
    auto ciphertext = Base64::decode(ciphertextBase64);
    if (ciphertext.size() != kem->length_ciphertext) {
        throw std::runtime_error("ML-KEM decapsulation failed: malformed ciphertext");
    }
    std::vector<uint8_t> sessionKey(kem->length_shared_secret);
    if (OQS_KEM_decaps(kem.get(), sessionKey.data(), ciphertext.data(), secretKey.data()) != OQS_SUCCESS) {
        throw std::runtime_error("ML-KEM decapsulation failed");
    }
    return sessionKey;
}

std::string PqcKeyExchangeService::encodePublicKey(const std::vector<uint8_t>& publicKey) {
    return Base64::encode(publicKey);
}

} // namespace legalchain::crypto
