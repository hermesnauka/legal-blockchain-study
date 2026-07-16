#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace legalchain::crypto {

/// Authenticated symmetric channel: AES-256-GCM under the ML-KEM-derived
/// session key.
///
/// GCM provides confidentiality *and* integrity in one primitive: any bit
/// flipped in transit fails the authentication tag and the message is
/// rejected, so a man-in-the-middle who cannot break the KEM cannot silently
/// alter ledger data either. A fresh random 96-bit IV is generated per
/// message and prepended to the ciphertext (an IV must never repeat under the
/// same GCM key; since our session keys are one-time ML-KEM secrets, random
/// IVs are safe at these message volumes).
class SecureChannel {
public:
    /// Encrypts UTF-8 plaintext; returns Base64(iv || ciphertext+tag).
    std::string encrypt(const std::string& plaintext, const std::vector<uint8_t>& sessionKey) const;

    /// Decrypts Base64(iv || ciphertext+tag); throws if the authentication tag is invalid.
    std::string decrypt(const std::string& payloadBase64, const std::vector<uint8_t>& sessionKey) const;

private:
    static constexpr int IV_BYTES = 12;
    static constexpr int TAG_BYTES = 16;
};

} // namespace legalchain::crypto
