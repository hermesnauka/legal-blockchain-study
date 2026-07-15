package com.devpowers.legalchain.crypto;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;
import java.util.List;

/**
 * SHA3-256 hashing utilities for block hashes and Merkle roots.
 *
 * <p>Why SHA3-256: Grover's algorithm gives a quantum attacker at most a quadratic
 * speed-up against hash pre-images, so a 256-bit sponge construction (Keccak) retains
 * ~128-bit post-quantum security. Unlike the signature layer, the hash layer therefore
 * needs no algorithm swap to stay quantum-resistant — which is why Bitcoin's SHA-256
 * chaining is not its quantum weakness (its ECDSA signatures are).</p>
 */
public final class HashUtil {

    private HashUtil() {
    }

    public static String sha3(String data) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA3-256");
            return HexFormat.of().formatHex(digest.digest(data.getBytes(StandardCharsets.UTF_8)));
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("SHA3-256 not available", e);
        }
    }

    /**
     * Computes a Merkle root over the given leaf hashes. The root commits to every
     * transaction in a block: changing any transaction changes the root, which changes
     * the block hash, which breaks every subsequent {@code previousHash} link — this is
     * what makes the ledger tamper-evident and auditable (a compliance requirement for
     * a general ledger).
     */
    public static String merkleRoot(List<String> leaves) {
        if (leaves.isEmpty()) {
            return sha3("EMPTY");
        }
        List<String> level = leaves.stream().map(HashUtil::sha3).toList();
        while (level.size() > 1) {
            List<String> next = new java.util.ArrayList<>(level.size() / 2 + 1);
            for (int i = 0; i < level.size(); i += 2) {
                String left = level.get(i);
                String right = (i + 1 < level.size()) ? level.get(i + 1) : left;
                next.add(sha3(left + right));
            }
            level = next;
        }
        return level.getFirst();
    }
}
