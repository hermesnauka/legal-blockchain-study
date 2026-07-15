package com.gandarych.legalchain.core;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Educational tokenomics: a fixed block reward that halves every N blocks under a hard
 * supply cap (the Bitcoin issuance model, scaled down so halvings are observable in a
 * classroom session).
 *
 * <p>Why this matters for compliance: the total supply and the issuance schedule are
 * deterministic functions of the chain itself — no discretionary "printing". An auditor
 * can recompute the entire monetary base from the ledger, which is exactly the
 * transparency property regulators (e.g. under MiCA) expect from a token issuer.</p>
 */
@Service
public class TokenomicsService {

    private final BigDecimal blockReward;
    private final int halvingInterval;
    private final BigDecimal maxSupply;

    public TokenomicsService(
            @Value("${legalchain.tokenomics.block-reward:50}") BigDecimal blockReward,
            @Value("${legalchain.tokenomics.halving-interval:100}") int halvingInterval,
            @Value("${legalchain.tokenomics.max-supply:21000}") BigDecimal maxSupply) {
        this.blockReward = blockReward;
        this.halvingInterval = halvingInterval;
        this.maxSupply = maxSupply;
    }

    /** Reward for the block at {@code blockIndex}, after halvings and the supply cap. */
    public BigDecimal rewardForBlock(long blockIndex, BigDecimal supplyAlreadyIssued) {
        int halvings = (int) (blockIndex / halvingInterval);
        BigDecimal reward = blockReward.divide(BigDecimal.TWO.pow(halvings));
        BigDecimal remaining = maxSupply.subtract(supplyAlreadyIssued);
        return reward.min(remaining).max(BigDecimal.ZERO);
    }

    /** Coinbase-style reward transaction crediting the sealing validator. */
    public Transaction rewardTransaction(String validatorAddress, long blockIndex,
                                         BigDecimal supplyAlreadyIssued) {
        return new Transaction(
                UUID.randomUUID().toString(),
                System.currentTimeMillis(),
                Transaction.SYSTEM,
                validatorAddress,
                rewardForBlock(blockIndex, supplyAlreadyIssued),
                TransactionType.REWARD,
                Map.of("reason", "block-reward", "block", String.valueOf(blockIndex)),
                null,
                null);
    }

    /** Total supply issued so far = sum of all REWARD transactions in the chain. */
    public BigDecimal issuedSupply(List<Block> chain) {
        return chain.stream()
                .flatMap(b -> b.transactions().stream())
                .filter(Transaction::isReward)
                .map(Transaction::amount)
                .reduce(BigDecimal.ZERO, BigDecimal::add);
    }
}
