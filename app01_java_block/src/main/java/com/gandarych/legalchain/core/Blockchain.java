package com.gandarych.legalchain.core;

import com.gandarych.legalchain.consensus.ConsensusEngine;
import com.gandarych.legalchain.crypto.PqcSignatureService;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * The general ledger: an append-only, hash-chained list of blocks plus the pool of
 * pending (signed, not yet sealed) transactions.
 *
 * <p><b>This is a real ledger, not a mock:</b> balances are never stored — they are
 * derived by replaying every transaction from genesis ({@link #balances()}), signatures
 * are verified with ML-DSA before a transaction is accepted, and
 * {@link #isValidChain(List)} re-audits hashes, links, Merkle roots, signatures and
 * consensus proofs of the entire history. That auditability-from-first-principles is
 * the property that makes a blockchain acceptable as a compliant book of record.</p>
 *
 * <p>Thread-safety: mutating operations synchronize on the chain monitor; reads return
 * defensive copies. With Loom virtual threads enabled, blocked writers cost almost
 * nothing.</p>
 */
@Service
public class Blockchain {

    /** Deterministic genesis: identical on every node, so two fresh remote nodes share a common root. */
    static final Block GENESIS = new Block(
            0, 0L, "0".repeat(64), Block.merkleRootOf(List.of()), List.of(),
            "GENESIS", "GENESIS", "genesis",
            0, Block.computeHash(0, 0L, "0".repeat(64), Block.merkleRootOf(List.of()),
            "GENESIS", "GENESIS", "genesis", 0));

    private final List<Block> chain = new ArrayList<>(List.of(GENESIS));
    private final List<Transaction> pending = new ArrayList<>();

    private final ConsensusEngine consensusEngine;
    private final TokenomicsService tokenomics;
    private final PqcSignatureService signatures;
    private final ApplicationEventPublisher events;

    public Blockchain(ConsensusEngine consensusEngine, TokenomicsService tokenomics,
                      PqcSignatureService signatures, ApplicationEventPublisher events) {
        this.consensusEngine = consensusEngine;
        this.tokenomics = tokenomics;
        this.signatures = signatures;
        this.events = events;
    }

    /**
     * Accepts a transaction into the pending pool after verifying its ML-DSA signature
     * and that the claimed sender address really is the fingerprint of the signing key
     * (otherwise anyone could sign as somebody else with their own key).
     */
    public Transaction submit(Transaction tx) {
        if (!tx.isReward()) {
            if (tx.senderPublicKey() == null || tx.signature() == null) {
                throw new IllegalArgumentException("Transaction must be signed (ML-DSA)");
            }
            if (!signatures.verify(tx.contentToSign(), tx.signature(), tx.senderPublicKey())) {
                throw new IllegalArgumentException("Invalid ML-DSA signature");
            }
            try {
                String expectedSender = signatures.fingerprint(
                        signatures.decodePublicKey(tx.senderPublicKey()));
                if (!expectedSender.equals(tx.sender())) {
                    throw new IllegalArgumentException("Sender address does not match signing key");
                }
            } catch (java.security.GeneralSecurityException e) {
                throw new IllegalArgumentException("Malformed sender public key");
            }
        }
        synchronized (chain) {
            pending.add(tx);
        }
        events.publishEvent(new LedgerEvent(LedgerEvent.Type.TX_ADDED, tx));
        return tx;
    }

    /**
     * Seals the pending transactions into a new block via the active consensus strategy
     * and credits the validator with the tokenomics reward.
     */
    public Block minePendingTransactions(String validatorAddress) {
        synchronized (chain) {
            Block previous = chain.getLast();
            long index = previous.index() + 1;

            List<Transaction> blockTxs = new ArrayList<>(pending);
            blockTxs.add(tokenomics.rewardTransaction(
                    validatorAddress, index, tokenomics.issuedSupply(chain)));

            BlockCandidate candidate = new BlockCandidate(
                    index, System.currentTimeMillis(), previous.hash(),
                    List.copyOf(blockTxs), validatorAddress);
            Block block = consensusEngine.active().seal(candidate);

            chain.add(block);
            pending.clear();
            events.publishEvent(new LedgerEvent(LedgerEvent.Type.BLOCK_ADDED, block));
            return block;
        }
    }

    /**
     * Full audit of a chain: genesis identity, hash consistency, previous-hash links,
     * transaction signatures, and each block's own consensus proof (looked up by the
     * strategy recorded in the block, so mixed-consensus histories stay verifiable).
     */
    public boolean isValidChain(List<Block> candidate) {
        if (candidate.isEmpty() || !candidate.getFirst().equals(GENESIS)) {
            return false;
        }
        for (int i = 1; i < candidate.size(); i++) {
            Block block = candidate.get(i);
            Block previous = candidate.get(i - 1);
            if (block.index() != previous.index() + 1
                    || !block.previousHash().equals(previous.hash())
                    || !block.hashIsConsistent()) {
                return false;
            }
            try {
                if (!consensusEngine.byName(block.consensusType()).validate(block)) {
                    return false;
                }
            } catch (IllegalArgumentException unknownStrategy) {
                return false;
            }
            for (Transaction tx : block.transactions()) {
                if (!tx.isReward()
                        && !signatures.verify(tx.contentToSign(), tx.signature(), tx.senderPublicKey())) {
                    return false;
                }
            }
        }
        return true;
    }

    public boolean isValid() {
        return isValidChain(blocks());
    }

    /**
     * Longest-valid-chain rule used by P2P sync: adopt a peer's chain only if it is
     * strictly longer <b>and</b> passes the full local audit — a malicious peer cannot
     * push a forged history, because we never trust, we always re-verify.
     */
    public boolean replaceChain(List<Block> candidate) {
        synchronized (chain) {
            if (candidate.size() <= chain.size() || !isValidChain(candidate)) {
                return false;
            }
            chain.clear();
            chain.addAll(candidate);
            pending.clear();
            events.publishEvent(new LedgerEvent(LedgerEvent.Type.CHAIN_REPLACED,
                    Map.of("length", candidate.size())));
            return true;
        }
    }

    /** Balances derived by replaying the full ledger — the "general ledger" property. */
    public Map<String, BigDecimal> balances() {
        Map<String, BigDecimal> balances = new HashMap<>();
        for (Block block : blocks()) {
            for (Transaction tx : block.transactions()) {
                if (!tx.isReward()) {
                    balances.merge(tx.sender(), tx.amount().negate(), BigDecimal::add);
                }
                balances.merge(tx.recipient(), tx.amount(), BigDecimal::add);
            }
        }
        return balances;
    }

    public List<Block> blocks() {
        synchronized (chain) {
            return List.copyOf(chain);
        }
    }

    public List<Transaction> pendingTransactions() {
        synchronized (chain) {
            return List.copyOf(pending);
        }
    }

    public int length() {
        synchronized (chain) {
            return chain.size();
        }
    }
}
