package com.devpowers.legalchain.consensus;

import com.devpowers.legalchain.core.Block;
import com.devpowers.legalchain.core.BlockCandidate;
import com.devpowers.legalchain.crypto.HashUtil;
import org.springframework.stereotype.Component;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Proof of Stake: the right to seal a block is assigned by stake-weighted lottery
 * instead of computational work.
 *
 * <p>The lottery must be <b>deterministic and grinding-resistant</b> so that every node
 * (including a peer receiving the block during sync) selects the same winner: we seed
 * it with the previous block hash — a value the sealer cannot freely re-roll — and walk
 * the cumulative stake distribution. Security intuition: attacking the network requires
 * owning a large share of the very asset the attack would devalue ("skin in the game"),
 * and misbehaviour can be punished by slashing stake. Energy cost is negligible, which
 * is why PoS is the default strategy of this compliance-oriented ledger (ESG reporting
 * is part of regulatory reality).</p>
 *
 * <p>MVP simplification: the validator set is a fixed, documented registry; production
 * systems derive it from on-chain staking transactions.</p>
 */
@Component
public class ProofOfStakeStrategy implements ConsensusStrategy {

    /** Demo validator registry: address → staked tokens (insertion order matters for determinism). */
    private final Map<String, Long> stakes = new LinkedHashMap<>(Map.of());

    public ProofOfStakeStrategy() {
        stakes.put("validator-alpha", 60L);
        stakes.put("validator-beta", 30L);
        stakes.put("validator-gamma", 10L);
    }

    @Override
    public String name() {
        return "POS";
    }

    @Override
    public String description() {
        return "Proof of Stake: deterministic stake-weighted lottery seeded by the previous "
                + "block hash; negligible energy use, security from economic stake.";
    }

    @Override
    public Block seal(BlockCandidate candidate) {
        String winner = selectValidator(candidate.previousHash());
        String proof = "stake-winner=" + winner + ";stake=" + stakes.get(winner)
                + ";seed=" + candidate.previousHash().substring(0, 12);
        return candidate.seal(name(), proof, 0);
    }

    @Override
    public boolean validate(Block block) {
        // Recompute the lottery from the same seed: the proof must name the same winner.
        String expectedWinner = selectValidator(block.previousHash());
        return block.proof().startsWith("stake-winner=" + expectedWinner + ";")
                && block.hashIsConsistent();
    }

    /** Deterministic stake-weighted selection seeded by the previous block hash. */
    private String selectValidator(String previousHash) {
        long totalStake = stakes.values().stream().mapToLong(Long::longValue).sum();
        // First 12 hex chars of SHA3(previousHash) → uniform draw in [0, totalStake).
        long draw = Long.parseLong(HashUtil.sha3(previousHash).substring(0, 12), 16) % totalStake;
        long cumulative = 0;
        for (Map.Entry<String, Long> entry : stakes.entrySet()) {
            cumulative += entry.getValue();
            if (draw < cumulative) {
                return entry.getKey();
            }
        }
        throw new IllegalStateException("Stake distribution exhausted — unreachable");
    }
}
