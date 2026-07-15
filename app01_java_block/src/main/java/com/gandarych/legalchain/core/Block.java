package com.gandarych.legalchain.core;

import com.gandarych.legalchain.crypto.HashUtil;

import java.util.List;

/**
 * Immutable block of the general ledger (Java record).
 *
 * <p>The block hash covers the index, timestamp, previous hash, Merkle root, validator,
 * consensus proof and nonce. Because {@code previousHash} of block N+1 is the hash of
 * block N, retroactively editing <i>any</i> historical transaction invalidates every
 * later block — this hash chaining is the core tamper-evidence property that makes a
 * blockchain acceptable as a compliant, auditable registry.</p>
 *
 * @param index         position in the chain (genesis = 0)
 * @param timestamp     epoch millis when the block was sealed
 * @param previousHash  hash of the preceding block (64 zeros for genesis)
 * @param merkleRoot    Merkle root committing to all transactions in this block
 * @param transactions  the transactions recorded in this block
 * @param validatorId   address of the node that sealed the block (receives the reward)
 * @param consensusType name of the consensus strategy that sealed this block
 * @param proof         strategy-specific proof (PoW target, PoS selection, BFT votes)
 * @param nonce         PoW counter (0 for non-PoW strategies)
 * @param hash          SHA3-256 hash of the block header
 */
public record Block(
        long index,
        long timestamp,
        String previousHash,
        String merkleRoot,
        List<Transaction> transactions,
        String validatorId,
        String consensusType,
        String proof,
        long nonce,
        String hash) {

    /** Recomputes the header hash; used both when sealing and when auditing the chain. */
    public static String computeHash(long index, long timestamp, String previousHash,
                                     String merkleRoot, String validatorId,
                                     String consensusType, String proof, long nonce) {
        return HashUtil.sha3(index + "|" + timestamp + "|" + previousHash + "|" + merkleRoot
                + "|" + validatorId + "|" + consensusType + "|" + proof + "|" + nonce);
    }

    public static String merkleRootOf(List<Transaction> transactions) {
        return HashUtil.merkleRoot(transactions.stream().map(Transaction::contentToSign).toList());
    }

    /** True if the stored hash matches a recomputation of the header — tamper check. */
    public boolean hashIsConsistent() {
        return hash.equals(computeHash(index, timestamp, previousHash,
                merkleRootOf(transactions), validatorId, consensusType, proof, nonce));
    }
}
