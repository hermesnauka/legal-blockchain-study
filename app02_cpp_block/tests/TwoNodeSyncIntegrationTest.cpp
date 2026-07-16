// Automated version of the two-node demo from the Definition of Done.
//
// Scope note vs. app01_java_block's TwoNodeSyncIntegrationTest: the Java test
// runs two full Spring Boot nodes (two ApplicationContexts) inside one JVM,
// connected over a real WebSocket. Drogon only supports one drogon::app()
// singleton per OS process, so two independently-listening node processes
// cannot both live inside this GoogleTest binary. This test instead proves
// the same properties at the layer below the WebSocket transport:
//  - BE-14: each node generates its own ML-DSA identity (fingerprints differ).
//  - BE-02: both nodes derive the identical deterministic genesis block.
//  - SEC-04: the real ML-DSA + ML-KEM handshake (byte-for-byte the same
//    message flow P2pService::onHello/onEncaps implements) establishes a
//    matching AES-256-GCM session key with no pre-shared secret.
//  - E4: the receiving node re-audits every block/signature/proof locally
//    before adopting a peer's chain (Blockchain::replaceChain).
// The WebSocket wire transport itself (P2pService/P2pClientService/
// P2pWebSocketController) is exercised by hand in the two-node curl demo
// documented in README.md, since that requires two real listening processes.
#include <gtest/gtest.h>

#include <memory>

#include "config/AppContext.h"
#include "crypto/PqcSignatureService.h"

using namespace legalchain;

TEST(TwoNodeSyncIntegrationTest, TwoRemoteNodesEstablishPqcChannelAndConvergeOnOneLedger) {
    config::Config configA;
    configA.nodeName = "sync-itest-A";
    config::AppContext nodeA(configA);

    config::Config configB;
    configB.nodeName = "sync-itest-B";
    config::AppContext nodeB(configB);

    // BE-14: each node generates its own ML-DSA identity — fingerprints differ.
    EXPECT_NE(nodeA.wallet().address(), nodeB.wallet().address());
    // BE-02: yet both derive the identical deterministic genesis block.
    EXPECT_EQ(nodeA.blockchain().blocks().front().hash(), nodeB.blockchain().blocks().front().hash());

    // Grow node A's ledger past node B's.
    nodeA.blockchain().minePendingTransactions(nodeA.wallet().address());
    nodeA.blockchain().minePendingTransactions(nodeA.wallet().address());
    int targetLength = nodeA.blockchain().length();
    ASSERT_EQ(targetLength, 3);
    ASSERT_EQ(nodeB.blockchain().length(), 1);

    // The real PQC handshake: B (initiator) generates an ephemeral ML-KEM
    // pair and signs it with its ML-DSA identity; A (responder) verifies the
    // signature, encapsulates a session key against B's KEM public key, and
    // signs the ciphertext; B decapsulates the same session key. This is
    // exactly the HELLO/ENCAPS exchange P2pService::onHello/onEncaps perform
    // over the wire — reproduced here directly, without the WebSocket hop.
    crypto::PqcSignatureService signatures;
    crypto::PqcKeyExchangeService keyExchange;

    auto kemPairB = keyExchange.generateKeyPair();
    std::string kemPubB = crypto::PqcKeyExchangeService::encodePublicKey(kemPairB.publicKey);
    std::string helloSignature = nodeB.wallet().sign(kemPubB + "|" + nodeB.wallet().address());

    // A verifies B's HELLO.
    ASSERT_TRUE(signatures.verify(kemPubB + "|" + nodeB.wallet().address(), helloSignature, nodeB.wallet().publicKeyBase64()));
    auto encapsulation = keyExchange.encapsulate(kemPubB);
    std::string encapsSignature = nodeA.wallet().sign(encapsulation.ciphertextBase64 + "|" + nodeA.wallet().address());

    // B verifies A's ENCAPS and decapsulates the same session key.
    ASSERT_TRUE(signatures.verify(encapsulation.ciphertextBase64 + "|" + nodeA.wallet().address(), encapsSignature,
                                   nodeA.wallet().publicKeyBase64()));
    auto sessionKeyB = keyExchange.decapsulate(encapsulation.ciphertextBase64, kemPairB.secretKey);

    EXPECT_EQ(sessionKeyB, encapsulation.sessionKey) << "both sides derive the identical one-time AES-256 key";
    EXPECT_EQ(sessionKeyB.size(), 32u);

    // Request the ledger over the (now proven-establishable) encrypted
    // channel; B re-audits before adopting — replaceChain never trusts the
    // sender, only the locally-recomputed hashes/signatures/proofs.
    bool adopted = nodeB.blockchain().replaceChain(nodeA.blockchain().blocks());

    EXPECT_TRUE(adopted);
    EXPECT_GE(nodeB.blockchain().length(), targetLength);
    EXPECT_TRUE(nodeB.blockchain().isValid()) << "adopted chain passes B's own full audit";
    EXPECT_EQ(nodeA.blockchain().blocks().back().hash(), nodeB.blockchain().blocks().back().hash());
}
