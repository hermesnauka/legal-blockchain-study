package com.devpowers.legalchain.core;

import java.math.BigDecimal;
import java.util.Map;
import java.util.TreeMap;

/**
 * Immutable ledger transaction (Java record — immutability is a compliance feature:
 * once created, an entry cannot be mutated in memory, only superseded on-chain).
 *
 * <p>Every non-reward transaction is signed with the sender's ML-DSA key over
 * {@link #contentToSign()}, and carries the sender's public key so that any node or
 * external auditor can verify authorship offline. The sender "address" is a SHA3-256
 * fingerprint of that key — pseudonymous, self-sovereign identity.</p>
 *
 * @param id              unique transaction id (UUID)
 * @param timestamp       epoch millis at creation
 * @param sender          sender address (key fingerprint) or {@code SYSTEM} for rewards
 * @param recipient       recipient address
 * @param amount          token amount (zero for pure data transactions)
 * @param type            ledger entry category
 * @param payload         type-specific data (NFT metadata, contract arguments, memo)
 * @param senderPublicKey Base64 X.509 ML-DSA public key ({@code null} for REWARD)
 * @param signature       Base64 ML-DSA signature over {@link #contentToSign()}
 */
public record Transaction(
        String id,
        long timestamp,
        String sender,
        String recipient,
        BigDecimal amount,
        TransactionType type,
        Map<String, String> payload,
        String senderPublicKey,
        String signature) {

    public static final String SYSTEM = "SYSTEM";

    /**
     * Canonical string covered by the ML-DSA signature. The payload map is serialized
     * in sorted-key order so signer and verifier always derive the same bytes.
     */
    public String contentToSign() {
        return id + '|' + timestamp + '|' + sender + '|' + recipient + '|'
                + amount.stripTrailingZeros().toPlainString() + '|' + type + '|'
                + new TreeMap<>(payload == null ? Map.of() : payload);
    }

    public boolean isReward() {
        return type == TransactionType.REWARD;
    }
}
