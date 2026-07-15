package com.devpowers.legalchain.consensus;

import com.devpowers.legalchain.core.Block;
import com.devpowers.legalchain.core.BlockCandidate;
import org.springframework.stereotype.Component;

/**
 * Proof of Work: the sealer increments a nonce until the block hash falls below a
 * difficulty target (here: a required prefix of zero hex digits).
 *
 * <p>Security intuition: rewriting history requires redoing the accumulated work of
 * every later block faster than the honest network extends it — economically
 * infeasible past a few confirmations. The proof is trivially verifiable (one hash)
 * but expensive to produce, an asymmetry that needs no identities at all.
 * Trade-off: enormous energy cost, which is why this MVP defaults to PoS and keeps
 * difficulty low (4 hex zeros ≈ 65k hashes) for classroom responsiveness.</p>
 */
@Component
public class ProofOfWorkStrategy implements ConsensusStrategy {

    static final String DIFFICULTY_PREFIX = "0000";

    @Override
    public String name() {
        return "POW";
    }

    @Override
    public String description() {
        return "Proof of Work: nonce search until the SHA3-256 block hash starts with "
                + DIFFICULTY_PREFIX + "; costly to produce, one hash to verify.";
    }

    @Override
    public Block seal(BlockCandidate candidate) {
        long nonce = 0;
        while (true) {
            Block block = candidate.seal(name(), "difficulty=" + DIFFICULTY_PREFIX.length(), nonce);
            if (block.hash().startsWith(DIFFICULTY_PREFIX)) {
                return block;
            }
            nonce++;
        }
    }

    @Override
    public boolean validate(Block block) {
        return block.hash().startsWith(DIFFICULTY_PREFIX) && block.hashIsConsistent();
    }
}
