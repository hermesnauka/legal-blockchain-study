#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace legalchain::crypto {

struct KemKeyPair {
    std::vector<uint8_t> publicKey;
    std::vector<uint8_t> secretKey;
};

/// Result of KEM encapsulation: what travels on the wire + the local session key.
struct Encapsulation {
    std::string ciphertextBase64;
    std::vector<uint8_t> sessionKey; // 32 bytes, used directly as an AES-256 key
};

/// Post-quantum key establishment using ML-KEM-768 (NIST FIPS 203,
/// standardized from CRYSTALS-Kyber) via liboqs.
///
/// QKD-inspired design. True Quantum Key Distribution needs dedicated optical
/// hardware, but its two security ideas can be reproduced in software: (1)
/// the shared symmetric key is freshly established per session and never
/// stored or reused, and (2) its secrecy does not depend on any key that a
/// "harvest now, decrypt later" adversary could break retroactively —
/// ML-KEM's security rests on the Module-LWE lattice problem, against which
/// Shor's algorithm gives no advantage. Each P2P session therefore gets a
/// one-time AES-256 key that a quantum-equipped eavesdropper recording
/// today's traffic still cannot recover.
///
/// MITM resistance comes from binding this KEM exchange to ML-DSA identities:
/// the handshake messages that carry the KEM public key and the ciphertext
/// are signed with the sender's ML-DSA key (see p2p::P2pService), so an
/// attacker cannot substitute their own KEM key without forging a
/// post-quantum signature.
class PqcKeyExchangeService {
public:
    static std::string algorithmName();

    KemKeyPair generateKeyPair() const;

    /// Sender side: encapsulates a fresh shared secret against the receiver's public key.
    Encapsulation encapsulate(const std::string& receiverPublicKeyBase64) const;

    /// Receiver side: recovers the same AES-256 session key from the transmitted ciphertext.
    std::vector<uint8_t> decapsulate(const std::string& ciphertextBase64,
                                      const std::vector<uint8_t>& secretKey) const;

    static std::string encodePublicKey(const std::vector<uint8_t>& publicKey);
};

} // namespace legalchain::crypto
