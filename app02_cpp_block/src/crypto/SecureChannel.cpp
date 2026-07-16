#include "crypto/SecureChannel.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <memory>
#include <stdexcept>

#include "crypto/Base64.h"

namespace legalchain::crypto {

namespace {
struct CtxDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const { EVP_CIPHER_CTX_free(ctx); }
};
using CtxPtr = std::unique_ptr<EVP_CIPHER_CTX, CtxDeleter>;
} // namespace

std::string SecureChannel::encrypt(const std::string& plaintext, const std::vector<uint8_t>& sessionKey) const {
    if (sessionKey.size() != 32) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte session key");
    }
    std::vector<uint8_t> iv(IV_BYTES);
    if (RAND_bytes(iv.data(), IV_BYTES) != 1) {
        throw std::runtime_error("Cannot generate random IV");
    }

    CtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("AES-GCM encryption failed: cannot allocate context");
    }
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, IV_BYTES, nullptr) != 1
        || EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, sessionKey.data(), iv.data()) != 1) {
        throw std::runtime_error("AES-GCM encryption failed: init");
    }

    std::vector<uint8_t> ciphertext(plaintext.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len,
                           reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size()) != 1) {
        throw std::runtime_error("AES-GCM encryption failed: update");
    }
    int totalLen = len;
    int finalLen = 0;
    if (EVP_EncryptFinal_ex(ctx.get(), ciphertext.data() + totalLen, &finalLen) != 1) {
        throw std::runtime_error("AES-GCM encryption failed: final");
    }
    totalLen += finalLen;
    ciphertext.resize(totalLen);

    std::vector<uint8_t> tag(TAG_BYTES);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_BYTES, tag.data()) != 1) {
        throw std::runtime_error("AES-GCM encryption failed: get tag");
    }

    std::vector<uint8_t> payload;
    payload.reserve(iv.size() + ciphertext.size() + tag.size());
    payload.insert(payload.end(), iv.begin(), iv.end());
    payload.insert(payload.end(), ciphertext.begin(), ciphertext.end());
    payload.insert(payload.end(), tag.begin(), tag.end());
    return Base64::encode(payload);
}

std::string SecureChannel::decrypt(const std::string& payloadBase64, const std::vector<uint8_t>& sessionKey) const {
    if (sessionKey.size() != 32) {
        throw std::runtime_error("AES-256-GCM requires a 32-byte session key");
    }
    auto payload = Base64::decode(payloadBase64);
    if (payload.size() < static_cast<std::size_t>(IV_BYTES + TAG_BYTES)) {
        throw std::runtime_error("AES-GCM decryption failed (tampered or wrong key)");
    }
    const uint8_t* iv = payload.data();
    const uint8_t* ciphertext = payload.data() + IV_BYTES;
    std::size_t ciphertextLen = payload.size() - IV_BYTES - TAG_BYTES;
    const uint8_t* tag = payload.data() + IV_BYTES + ciphertextLen;

    CtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("AES-GCM decryption failed: cannot allocate context");
    }
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1
        || EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, IV_BYTES, nullptr) != 1
        || EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, sessionKey.data(), iv) != 1) {
        throw std::runtime_error("AES-GCM decryption failed: init");
    }

    std::vector<uint8_t> plaintext(ciphertextLen);
    int len = 0;
    if (ciphertextLen > 0
        && EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len, ciphertext, ciphertextLen) != 1) {
        throw std::runtime_error("AES-GCM decryption failed (tampered or wrong key)");
    }
    int totalLen = len;
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_BYTES, const_cast<uint8_t*>(tag)) != 1) {
        throw std::runtime_error("AES-GCM decryption failed: set tag");
    }
    int finalLen = 0;
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + totalLen, &finalLen) != 1) {
        throw std::runtime_error("AES-GCM decryption failed (tampered or wrong key)");
    }
    totalLen += finalLen;
    plaintext.resize(totalLen);
    return std::string(plaintext.begin(), plaintext.end());
}

} // namespace legalchain::crypto
