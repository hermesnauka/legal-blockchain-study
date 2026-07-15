package com.gandarych.legalchain.contracts;

import java.util.Map;

/**
 * Blockchain 3.0 smart contract abstraction: deterministic business logic whose every
 * execution is recorded as a signed ledger transaction.
 *
 * <p>Design rule for compliance (GDPR art. 17 "right to erasure" vs. immutability):
 * contracts must keep <b>personal data off-chain</b> and record on-chain only hashes,
 * pseudonymous identifiers and consent/State transitions. The chain then proves
 * <i>that</i> and <i>when</i> something happened without ever containing erasable
 * personal data — the standard pattern that reconciles blockchains with GDPR.</p>
 */
public interface SmartContract {

    /** Stable contract identifier recorded in the transaction payload. */
    String contractId();

    /** Sector / purpose description surfaced by the educational frontend. */
    String description();

    /**
     * Validates the input against the contract's rules and returns the state transition
     * to be recorded on-chain. Implementations must be deterministic and side-effect
     * free apart from their own state registry (same input → same result on every node).
     *
     * @throws IllegalArgumentException if the input violates contract rules
     */
    ContractResult execute(Map<String, String> input);
}
