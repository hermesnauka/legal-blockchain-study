package com.devpowers.legalchain.core;

import com.devpowers.legalchain.consensus.BftStrategy;
import com.devpowers.legalchain.consensus.ConsensusEngine;
import com.devpowers.legalchain.consensus.ProofOfStakeStrategy;
import com.devpowers.legalchain.consensus.ProofOfWorkStrategy;
import com.devpowers.legalchain.crypto.PqcSignatureService;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.math.BigDecimal;
import java.security.KeyPair;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.UUID;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Proves the "real ledger, not a mock" properties: signed submission, sealing with
 * rewards, full-chain audit, tamper detection and the longest-valid-chain sync rule.
 */
class BlockchainTest {

    private final PqcSignatureService signatures = new PqcSignatureService();
    private Blockchain blockchain;
    private KeyPair sender;
    private String senderAddress;

    @BeforeEach
    void setUp() {
        ConsensusEngine engine = new ConsensusEngine(
                List.of(new ProofOfWorkStrategy(), new ProofOfStakeStrategy(), new BftStrategy()),
                "POS");
        TokenomicsService tokenomics = new TokenomicsService(
                new BigDecimal("50"), 100, new BigDecimal("21000"));
        blockchain = new Blockchain(engine, tokenomics, signatures, event -> {
        });
        sender = signatures.generateKeyPair();
        senderAddress = signatures.fingerprint(sender.getPublic());
    }

    private Transaction signedTransfer(String recipient, String amount) {
        Transaction unsigned = new Transaction(
                UUID.randomUUID().toString(), System.currentTimeMillis(),
                senderAddress, recipient, new BigDecimal(amount),
                TransactionType.TRANSFER, Map.of(),
                signatures.encodePublicKey(sender.getPublic()), null);
        return new Transaction(
                unsigned.id(), unsigned.timestamp(), unsigned.sender(), unsigned.recipient(),
                unsigned.amount(), unsigned.type(), unsigned.payload(),
                unsigned.senderPublicKey(), signatures.sign(unsigned.contentToSign(), sender.getPrivate()));
    }

    @Test
    void startsWithDeterministicGenesis() {
        assertThat(blockchain.length()).isEqualTo(1);
        assertThat(blockchain.blocks().getFirst().previousHash()).isEqualTo("0".repeat(64));
        assertThat(blockchain.isValid()).isTrue();
    }

    @Test
    void rejectsUnsignedAndForgedTransactions() {
        Transaction unsigned = new Transaction(
                UUID.randomUUID().toString(), System.currentTimeMillis(),
                senderAddress, "bob", BigDecimal.ONE,
                TransactionType.TRANSFER, Map.of(), null, null);
        assertThatThrownBy(() -> blockchain.submit(unsigned))
                .isInstanceOf(IllegalArgumentException.class);

        // Valid signature but claimed sender is not the signing key's fingerprint.
        Transaction impersonation = new Transaction(
                UUID.randomUUID().toString(), System.currentTimeMillis(),
                "someone-else", "bob", BigDecimal.ONE,
                TransactionType.TRANSFER, Map.of(),
                signatures.encodePublicKey(sender.getPublic()), null);
        Transaction signedImpersonation = new Transaction(
                impersonation.id(), impersonation.timestamp(), impersonation.sender(),
                impersonation.recipient(), impersonation.amount(), impersonation.type(),
                impersonation.payload(), impersonation.senderPublicKey(),
                signatures.sign(impersonation.contentToSign(), sender.getPrivate()));
        assertThatThrownBy(() -> blockchain.submit(signedImpersonation))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void minesBlockWithRewardAndDerivesBalances() {
        blockchain.submit(signedTransfer("bob", "3"));
        Block block = blockchain.minePendingTransactions("validator-x");

        assertThat(block.index()).isEqualTo(1);
        assertThat(block.transactions()).hasSize(2); // transfer + reward
        assertThat(blockchain.isValid()).isTrue();
        assertThat(blockchain.pendingTransactions()).isEmpty();
        assertThat(blockchain.balances())
                .containsEntry("bob", new BigDecimal("3"))
                .containsEntry("validator-x", new BigDecimal("50"));
    }

    @Test
    void detectsTamperedHistory() {
        blockchain.submit(signedTransfer("bob", "3"));
        blockchain.minePendingTransactions("validator-x");

        // Rewrite history: replace the transfer with a doubled amount, keep everything else.
        List<Block> tampered = new ArrayList<>(blockchain.blocks());
        Block original = tampered.get(1);
        Transaction tx = original.transactions().getFirst();
        Transaction doubled = new Transaction(tx.id(), tx.timestamp(), tx.sender(),
                tx.recipient(), new BigDecimal("6"), tx.type(), tx.payload(),
                tx.senderPublicKey(), tx.signature());
        tampered.set(1, new Block(original.index(), original.timestamp(), original.previousHash(),
                original.merkleRoot(), List.of(doubled, original.transactions().get(1)),
                original.validatorId(), original.consensusType(), original.proof(),
                original.nonce(), original.hash()));

        assertThat(blockchain.isValidChain(tampered)).isFalse();
    }

    @Test
    void replaceChainAdoptsOnlyLongerValidChains() {
        // Build a longer chain in a second ledger instance.
        ConsensusEngine engine = new ConsensusEngine(
                List.of(new ProofOfWorkStrategy(), new ProofOfStakeStrategy(), new BftStrategy()),
                "POS");
        Blockchain remote = new Blockchain(engine,
                new TokenomicsService(new BigDecimal("50"), 100, new BigDecimal("21000")),
                signatures, event -> {
        });
        remote.minePendingTransactions("remote-validator");
        remote.minePendingTransactions("remote-validator");

        assertThat(blockchain.replaceChain(remote.blocks())).isTrue();
        assertThat(blockchain.length()).isEqualTo(3);

        // A shorter chain must be refused.
        assertThat(blockchain.replaceChain(List.of(blockchain.blocks().getFirst()))).isFalse();
    }
}
