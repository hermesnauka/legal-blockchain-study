// Proves the "real ledger, not a mock" properties: signed submission, sealing
// with rewards, full-chain audit, tamper detection and the longest-valid-chain
// sync rule. Mirrors app01_java_block's BlockchainTest.java.
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "consensus/BftStrategy.h"
#include "consensus/ConsensusEngine.h"
#include "consensus/ProofOfStakeStrategy.h"
#include "consensus/ProofOfWorkStrategy.h"
#include "core/Blockchain.h"
#include "core/IdGen.h"
#include "crypto/PqcSignatureService.h"

using namespace legalchain;

namespace {
std::vector<std::unique_ptr<consensus::ConsensusStrategy>> allStrategies() {
    std::vector<std::unique_ptr<consensus::ConsensusStrategy>> v;
    v.push_back(std::make_unique<consensus::ProofOfWorkStrategy>());
    v.push_back(std::make_unique<consensus::ProofOfStakeStrategy>());
    v.push_back(std::make_unique<consensus::BftStrategy>());
    return v;
}
} // namespace

class BlockchainTest : public ::testing::Test {
protected:
    crypto::PqcSignatureService signatures;
    core::EventBus events;
    std::unique_ptr<consensus::ConsensusEngine> engine;
    std::unique_ptr<core::Blockchain> blockchain;
    crypto::SignatureKeyPair sender;
    std::string senderAddress;

    void SetUp() override {
        engine = std::make_unique<consensus::ConsensusEngine>(allStrategies(), "POS");
        core::TokenomicsService tokenomics(50.0, 100, 21000.0);
        blockchain = std::make_unique<core::Blockchain>(*engine, tokenomics, signatures, events);
        sender = signatures.generateKeyPair();
        senderAddress = crypto::PqcSignatureService::fingerprint(sender.publicKey);
    }

    core::Transaction signedTransfer(const std::string& recipient, double amount) {
        core::Transaction unsigned_(core::newUuid(), core::nowMillis(), senderAddress, recipient, amount,
                                     core::TransactionType::TRANSFER, {},
                                     crypto::PqcSignatureService::encodePublicKey(sender.publicKey), std::nullopt);
        core::Transaction signed_(unsigned_.id(), unsigned_.timestampMs(), unsigned_.sender(),
                                   unsigned_.recipient(), unsigned_.amount(), unsigned_.type(),
                                   unsigned_.payload(), unsigned_.senderPublicKey(),
                                   signatures.sign(unsigned_.contentToSign(), sender.secretKey));
        return signed_;
    }
};

TEST_F(BlockchainTest, StartsWithDeterministicGenesis) {
    EXPECT_EQ(blockchain->length(), 1);
    EXPECT_EQ(blockchain->blocks().front().previousHash(), std::string(64, '0'));
    EXPECT_TRUE(blockchain->isValid());
}

TEST_F(BlockchainTest, RejectsUnsignedAndForgedTransactions) {
    core::Transaction unsigned_(core::newUuid(), core::nowMillis(), senderAddress, "bob", 1.0,
                                 core::TransactionType::TRANSFER, {}, std::nullopt, std::nullopt);
    EXPECT_THROW(blockchain->submit(unsigned_), std::invalid_argument);

    // Valid signature but claimed sender is not the signing key's fingerprint.
    core::Transaction impersonation(core::newUuid(), core::nowMillis(), "someone-else", "bob", 1.0,
                                     core::TransactionType::TRANSFER, {},
                                     crypto::PqcSignatureService::encodePublicKey(sender.publicKey), std::nullopt);
    core::Transaction signedImpersonation(
        impersonation.id(), impersonation.timestampMs(), impersonation.sender(), impersonation.recipient(),
        impersonation.amount(), impersonation.type(), impersonation.payload(), impersonation.senderPublicKey(),
        signatures.sign(impersonation.contentToSign(), sender.secretKey));
    EXPECT_THROW(blockchain->submit(signedImpersonation), std::invalid_argument);
}

TEST_F(BlockchainTest, MinesBlockWithRewardAndDerivesBalances) {
    blockchain->submit(signedTransfer("bob", 3.0));
    core::Block block = blockchain->minePendingTransactions("validator-x");

    EXPECT_EQ(block.index(), 1);
    EXPECT_EQ(block.transactions().size(), 2u); // transfer + reward
    EXPECT_TRUE(blockchain->isValid());
    EXPECT_TRUE(blockchain->pendingTransactions().empty());

    auto balances = blockchain->balances();
    EXPECT_DOUBLE_EQ(balances["bob"], 3.0);
    EXPECT_DOUBLE_EQ(balances["validator-x"], 50.0);
}

TEST_F(BlockchainTest, DetectsTamperedHistory) {
    blockchain->submit(signedTransfer("bob", 3.0));
    blockchain->minePendingTransactions("validator-x");

    // Rewrite history: replace the transfer with a doubled amount, keep everything else.
    std::vector<core::Block> tampered = blockchain->blocks();
    core::Block original = tampered[1];
    core::Transaction tx = original.transactions().front();
    core::Transaction doubled(tx.id(), tx.timestampMs(), tx.sender(), tx.recipient(), 6.0, tx.type(), tx.payload(),
                               tx.senderPublicKey(), tx.signature());
    std::vector<core::Transaction> txs = {doubled, original.transactions()[1]};
    tampered[1] = core::Block(original.index(), original.timestampMs(), original.previousHash(),
                               original.merkleRoot(), txs, original.validatorId(), original.consensusType(),
                               original.proof(), original.nonce(), original.hash());

    EXPECT_FALSE(blockchain->isValidChain(tampered));
}

TEST_F(BlockchainTest, ReplaceChainAdoptsOnlyLongerValidChains) {
    // Build a longer chain in a second ledger instance.
    auto remoteEngine = std::make_unique<consensus::ConsensusEngine>(allStrategies(), "POS");
    core::TokenomicsService tokenomics(50.0, 100, 21000.0);
    core::EventBus remoteEvents;
    core::Blockchain remote(*remoteEngine, tokenomics, signatures, remoteEvents);
    remote.minePendingTransactions("remote-validator");
    remote.minePendingTransactions("remote-validator");

    EXPECT_TRUE(blockchain->replaceChain(remote.blocks()));
    EXPECT_EQ(blockchain->length(), 3);

    // A shorter chain must be refused.
    std::vector<core::Block> shortChain = {blockchain->blocks().front()};
    EXPECT_FALSE(blockchain->replaceChain(shortChain));
}
