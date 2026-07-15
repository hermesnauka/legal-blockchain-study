package com.gandarych.legalchain.p2p;

import com.gandarych.legalchain.LegalChainApplication;
import com.gandarych.legalchain.core.Blockchain;
import com.gandarych.legalchain.core.NodeWallet;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.builder.SpringApplicationBuilder;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.server.LocalServerPort;
import org.springframework.context.ConfigurableApplicationContext;

import java.util.function.BooleanSupplier;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.fail;

/**
 * Automated version of the two-node demo from the Definition of Done: two full
 * Spring Boot nodes in one JVM, connected over a real WebSocket with the real
 * ML-DSA + ML-KEM handshake, converging on one ledger.
 *
 * <p>What this proves end-to-end: distinct self-sovereign node identities (BE-14),
 * a deterministic shared genesis (BE-02), a PQC-secured channel established without
 * any pre-shared secret (SEC-04), and full-chain adoption where the receiving node
 * re-audits every block, signature and proof locally before accepting (E4).</p>
 */
@SpringBootTest(
        webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT,
        properties = "legalchain.node.name=sync-itest-A")
class TwoNodeSyncIntegrationTest {

    @LocalServerPort
    private int portA;

    @Autowired
    private Blockchain chainA;
    @Autowired
    private NodeWallet walletA;
    @Autowired
    private P2pService p2pA;

    @Test
    void twoRemoteNodesEstablishPqcChannelAndConvergeOnOneLedger() {
        ConfigurableApplicationContext nodeB = new SpringApplicationBuilder(LegalChainApplication.class)
                .properties("server.port=0", "legalchain.node.name=sync-itest-B")
                .run();
        try {
            Blockchain chainB = nodeB.getBean(Blockchain.class);
            NodeWallet walletB = nodeB.getBean(NodeWallet.class);

            // BE-14: each node generates its own ML-DSA identity — fingerprints differ.
            assertNotEquals(walletA.address(), walletB.address());
            // BE-02: yet both derive the identical deterministic genesis block.
            assertEquals(chainA.blocks().getFirst().hash(), chainB.blocks().getFirst().hash());

            // Grow node A's ledger past node B's.
            chainA.minePendingTransactions(walletA.address());
            chainA.minePendingTransactions(walletA.address());
            int targetLength = chainA.length();

            // B dials A: WebSocket + ML-DSA-signed hello + ML-KEM encapsulation.
            nodeB.getBean(P2pClientService.class).connect("ws://localhost:" + portA + "/ws/p2p");
            P2pService p2pB = nodeB.getBean(P2pService.class);
            await("PQC handshake on both sides",
                    () -> !p2pB.connectedPeers().isEmpty() && !p2pA.connectedPeers().isEmpty());

            // Request the ledger over the encrypted channel; B re-audits before adopting.
            p2pB.syncWithPeers();
            await("chain convergence",
                    () -> chainB.length() >= targetLength
                            && chainB.blocks().getLast().hash().equals(chainA.blocks().getLast().hash()));

            assertTrue(chainB.isValid(), "adopted chain passes B's own full audit");
            assertEquals(chainA.blocks().getLast().hash(), chainB.blocks().getLast().hash());
        } finally {
            nodeB.close();
        }
    }

    /** Polls the condition for up to 20 s — handshake and sync are asynchronous. */
    private static void await(String what, BooleanSupplier condition) {
        long deadline = System.currentTimeMillis() + 20_000;
        while (System.currentTimeMillis() < deadline) {
            if (condition.getAsBoolean()) {
                return;
            }
            try {
                Thread.sleep(200);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                fail("interrupted while waiting for " + what);
            }
        }
        fail("timed out after 20s waiting for " + what);
    }
}
