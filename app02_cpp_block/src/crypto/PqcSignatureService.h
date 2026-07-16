#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace legalchain::crypto {

/// A generated ML-DSA-65 key pair (raw bytes, as produced by liboqs).
struct SignatureKeyPair {
    std::vector<uint8_t> publicKey;
    std::vector<uint8_t> secretKey;
};

/// Post-quantum digital signatures using ML-DSA-65 (NIST FIPS 204,
/// standardized from CRYSTALS-Dilithium), provided by liboqs.
///
/// Why this is compliant and secure: classical blockchain signatures
/// (Bitcoin/Ethereum ECDSA over secp256k1) rely on the discrete-logarithm
/// problem, which Shor's algorithm solves in polynomial time on a large
/// fault-tolerant quantum computer. ML-DSA instead rests on the hardness of
/// lattice problems (Module-LWE / Module-SIS), for which no quantum speed-up
/// is known. NIST finalized it as FIPS 204 in August 2024; using a
/// NIST-standardized parameter set (ML-DSA-65, NIST security category 3,
/// ~AES-192 equivalent) is what makes the ledger auditable against a
/// concrete regulatory benchmark rather than ad-hoc cryptography.
///
/// Every ledger transaction carries the sender's ML-DSA public key and
/// signature, so any node — or any auditor — can independently verify
/// authorship without contacting the author. Identity stays pseudonymous:
/// the "address" is only a SHA3-256 fingerprint of the public key
/// (self-sovereign identity), never a name.
class PqcSignatureService {
public:
    /// Human-readable algorithm label exposed over the API and in the education module.
    static std::string algorithmName();

    SignatureKeyPair generateKeyPair() const;

    /// Signs UTF-8 content and returns the signature as Base64.
    std::string sign(const std::string& content, const std::vector<uint8_t>& secretKey) const;

    /// Verifies a Base64 signature against UTF-8 content and a Base64-encoded public key.
    bool verify(const std::string& content, const std::string& signatureBase64,
                const std::string& publicKeyBase64) const;

    std::vector<uint8_t> decodePublicKey(const std::string& publicKeyBase64) const;
    static std::string encodePublicKey(const std::vector<uint8_t>& publicKey);

    /// Pseudonymous address: SHA3-256 fingerprint of the encoded public key.
    /// This is the ZKP-flavoured privacy property of the ledger — parties
    /// prove control of a key (by producing valid ML-DSA signatures) without
    /// ever revealing a real-world identity.
    static std::string fingerprint(const std::vector<uint8_t>& publicKey);
};

} // namespace legalchain::crypto
