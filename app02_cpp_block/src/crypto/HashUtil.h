#pragma once

#include <string>
#include <vector>

namespace legalchain::crypto {

/// SHA3-256 hashing utilities for block hashes and Merkle roots.
///
/// Why SHA3-256: Grover's algorithm gives a quantum attacker at most a
/// quadratic speed-up against hash pre-images, so a 256-bit sponge
/// construction (Keccak) retains ~128-bit post-quantum security. Unlike the
/// signature layer, the hash layer therefore needs no algorithm swap to stay
/// quantum-resistant — which is why Bitcoin's SHA-256 chaining is not its
/// quantum weakness (its ECDSA signatures are).
class HashUtil {
public:
    static std::string sha3(const std::string& data);

    /// Computes a Merkle root over the given leaves. The root commits to
    /// every transaction in a block: changing any transaction changes the
    /// root, which changes the block hash, which breaks every subsequent
    /// previousHash link — this is what makes the ledger tamper-evident and
    /// auditable (a compliance requirement for a general ledger).
    static std::string merkleRoot(const std::vector<std::string>& leaves);
};

} // namespace legalchain::crypto
