package com.devpowers.legalchain.consensus;

import com.devpowers.legalchain.core.Block;
import com.devpowers.legalchain.core.BlockCandidate;

/**
 * Strategy pattern for consensus: the ledger core is agnostic about <i>how</i> a block
 * earns the right to be appended. Each implementation seals a candidate (producing its
 * proof) and can independently re-validate any sealed block — validation must be a pure
 * function of the block, because remote nodes re-check proofs during P2P sync without
 * trusting the sender.
 *
 * <p>This is the "Consensus and Scalability" seam of the architecture: adding DPoS,
 * Proof-of-Reputation, PoH or PoET later means adding one class, not touching the core.</p>
 */
public interface ConsensusStrategy {

    /** Stable identifier used in the API and stored in each block ({@code consensusType}). */
    String name();

    /** One-line explanation surfaced by the educational frontend. */
    String description();

    /** Produces the proof for the candidate and returns the sealed, hashed block. */
    Block seal(BlockCandidate candidate);

    /** Re-verifies the proof of a sealed block (used when auditing chains from peers). */
    boolean validate(Block block);
}
