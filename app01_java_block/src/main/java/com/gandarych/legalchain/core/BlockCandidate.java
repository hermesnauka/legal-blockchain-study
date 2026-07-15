package com.gandarych.legalchain.core;

import java.util.List;

/**
 * An unsealed block handed to a {@link com.gandarych.legalchain.consensus.ConsensusStrategy},
 * which produces the proof/nonce and the final {@link Block}. Separating the candidate
 * from the sealed block is what lets consensus algorithms be swapped (Strategy pattern)
 * without touching the ledger core.
 */
public record BlockCandidate(
        long index,
        long timestamp,
        String previousHash,
        List<Transaction> transactions,
        String validatorId) {

    public String merkleRoot() {
        return Block.merkleRootOf(transactions);
    }

    /** Convenience for strategies: seal this candidate with the given proof and nonce. */
    public Block seal(String consensusType, String proof, long nonce) {
        String merkleRoot = merkleRoot();
        String hash = Block.computeHash(index, timestamp, previousHash, merkleRoot,
                validatorId, consensusType, proof, nonce);
        return new Block(index, timestamp, previousHash, merkleRoot, transactions,
                validatorId, consensusType, proof, nonce, hash);
    }
}
