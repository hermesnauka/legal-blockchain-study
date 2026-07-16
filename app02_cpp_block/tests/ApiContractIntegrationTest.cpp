// Contract-level integration test for API Contract v1 (checklist BE-15).
//
// Scope note vs. app01_java_block's ApiContractIntegrationTest: the Java test
// drives a real embedded HTTP server end to end. Drogon only supports one
// drogon::app() singleton per process, so spinning up a real listening node
// inside the same GoogleTest binary as the other suites is impractical here.
// Instead this test wires a real config::AppContext (the same object graph
// main.cpp builds — real ML-DSA signing, real consensus, real contracts) and
// drives it through the exact sequence of operations each api/*.cpp handler
// performs, asserting the same outcomes the Java test checks. What is NOT
// covered here (and is covered instead by manual verification, see the
// project README): HTTP status codes, JSON wire framing, and CORS headers —
// those are mechanical and already exercised by hand in the two-node curl
// demo.
#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include "config/AppContext.h"
#include "core/IdGen.h"

using namespace legalchain;

namespace {
core::Transaction signedTransfer(config::AppContext& ctx, const std::string& recipient, double amount) {
    auto& wallet = ctx.wallet();
    core::Transaction unsigned_(core::newUuid(), core::nowMillis(), wallet.address(), recipient, amount,
                                 core::TransactionType::TRANSFER, {}, wallet.publicKeyBase64(), std::nullopt);
    return core::Transaction(unsigned_.id(), unsigned_.timestampMs(), unsigned_.sender(), unsigned_.recipient(),
                              unsigned_.amount(), unsigned_.type(), unsigned_.payload(),
                              unsigned_.senderPublicKey(), wallet.sign(unsigned_.contentToSign()));
}
} // namespace

TEST(ApiContractIntegrationTest, FullAuditableSessionMatchesContractV1) {
    config::Config cfg;
    cfg.nodeName = "api-itest";
    cfg.port = 0;
    config::AppContext ctx(cfg);

    // (1) Node identity contract.
    std::string nodeId = ctx.wallet().address();
    EXPECT_GT(nodeId.size(), 16u) << "nodeId is a key fingerprint";
    EXPECT_EQ(ctx.blockchain().length(), 1) << "fresh node starts at genesis";

    // (2) Genesis + full audit.
    auto blocks = ctx.blockchain().blocks();
    EXPECT_EQ(blocks.size(), 1u);
    EXPECT_TRUE(ctx.blockchain().isValid());
    EXPECT_EQ(blocks.front().index(), 0);
    EXPECT_EQ(blocks.front().previousHash(), std::string(64, '0'));

    // (3) Wallet exposes only pseudonymous identity, never key material by
    // construction (NodeWallet has no accessor that returns the secret key).
    EXPECT_FALSE(ctx.wallet().publicKeyBase64().empty());
    EXPECT_NE(ctx.wallet().algorithm().find("ML-DSA"), std::string::npos);

    // (4) Overdraft is rejected and never reaches the mempool (BE-03).
    EXPECT_THROW(ctx.blockchain().submit(signedTransfer(ctx, "mallory", 999999.0)), std::invalid_argument);
    EXPECT_TRUE(ctx.blockchain().pendingTransactions().empty());

    // (5) Mining seals a reward transaction via the active (default POS) consensus.
    core::Block block1 = ctx.blockchain().minePendingTransactions(ctx.wallet().address());
    EXPECT_EQ(block1.index(), 1);
    EXPECT_EQ(block1.consensusType(), "POS");
    ASSERT_EQ(block1.transactions().size(), 1u);
    EXPECT_EQ(block1.transactions().front().type(), core::TransactionType::REWARD);
    EXPECT_EQ(block1.transactions().front().sender(), core::Transaction::SYSTEM);
    EXPECT_EQ(block1.transactions().front().recipient(), nodeId);
    EXPECT_DOUBLE_EQ(block1.transactions().front().amount(), 50.0);

    // (6) Signed transfer flows through the mempool into balances.
    core::Transaction tx = ctx.blockchain().submit(signedTransfer(ctx, "alice", 2.0));
    EXPECT_EQ(tx.type(), core::TransactionType::TRANSFER);
    ASSERT_TRUE(tx.signature().has_value());
    ASSERT_TRUE(tx.senderPublicKey().has_value());
    EXPECT_EQ(ctx.blockchain().pendingTransactions().size(), 1u);

    core::Block block2 = ctx.blockchain().minePendingTransactions(ctx.wallet().address());
    EXPECT_EQ(block2.transactions().size(), 2u);
    EXPECT_TRUE(ctx.blockchain().pendingTransactions().empty());

    auto balances = ctx.blockchain().balances();
    EXPECT_DOUBLE_EQ(balances["alice"], 2.0);
    EXPECT_DOUBLE_EQ(balances[nodeId], 98.0); // 2x50 reward - 2 sent

    // (7) Consensus hot-swap shapes the next block.
    auto available = ctx.consensus().available();
    std::set<std::string> names;
    for (auto* strategy : available) names.insert(strategy->name());
    EXPECT_TRUE(names.count("POW") && names.count("POS") && names.count("BFT"));

    ctx.consensus().switchTo("BFT");
    EXPECT_EQ(ctx.consensus().active().name(), "BFT");
    core::Block block3 = ctx.blockchain().minePendingTransactions(ctx.wallet().address());
    EXPECT_EQ(block3.consensusType(), "BFT");
    EXPECT_NE(block3.proof().find("quorum"), std::string::npos);
    ctx.consensus().switchTo("POS");

    // (8) NFT mint binds the token to the creator's fingerprint.
    auto nft = ctx.nftService().mint("Integration Artwork", "minted by the API contract test",
                                      "ipfs://QmIntegration");
    EXPECT_FALSE(nft.tokenId.empty());
    EXPECT_FALSE(nft.txId.empty());
    EXPECT_EQ(nft.creator, nodeId);
    ctx.blockchain().minePendingTransactions(ctx.wallet().address());
    auto allNfts = ctx.nftService().all();
    EXPECT_TRUE(std::any_of(allNfts.begin(), allNfts.end(),
                             [&](const nft::Nft& n) { return n.tokenId == nft.tokenId; }));

    // (9) Medical consent lifecycle: data-minimal, auditable.
    auto grant = ctx.contractEngine().execute(
        ctx.medical(), core::TransactionType::CONTRACT_MEDICAL,
        {{"patientId", "itest-patient"}, {"granteeId", "dr-house"}, {"scope", "LAB_RESULTS"}, {"granted", "true"}});
    EXPECT_TRUE(grant.accepted);
    std::set<std::string> recordKeys;
    for (const auto& [k, v] : grant.record) recordKeys.insert(k);
    EXPECT_EQ(recordKeys, (std::set<std::string>{"contract", "patientId", "granteeId", "scope", "granted"}));

    ctx.contractEngine().execute(
        ctx.medical(), core::TransactionType::CONTRACT_MEDICAL,
        {{"patientId", "itest-patient"}, {"granteeId", "dr-house"}, {"scope", "LAB_RESULTS"}, {"granted", "false"}});
    auto history = ctx.medical().historyOf("itest-patient");
    ASSERT_EQ(history.size(), 2u);
    EXPECT_TRUE(history.front().granted);
    EXPECT_FALSE(history.back().granted);

    EXPECT_THROW(ctx.contractEngine().execute(
                     ctx.medical(), core::TransactionType::CONTRACT_MEDICAL,
                     {{"patientId", "itest-patient"}, {"granteeId", "dr-house"}, {"scope", "TELEPATHY"},
                      {"granted", "true"}}),
                 std::invalid_argument);
    ctx.blockchain().minePendingTransactions(ctx.wallet().address());

    // (10) Agri trail is chronological and forward-only.
    ctx.contractEngine().execute(
        ctx.agri(), core::TransactionType::CONTRACT_AGRI,
        {{"batchId", "IT-BATCH-1"}, {"stage", "HARVEST"}, {"actor", "Green Farm"}, {"location", "Lublin"},
         {"details", "organic"}});
    ctx.contractEngine().execute(
        ctx.agri(), core::TransactionType::CONTRACT_AGRI,
        {{"batchId", "IT-BATCH-1"}, {"stage", "TRANSPORT"}, {"actor", "ColdTrans"}, {"location", "Warszawa"},
         {"details", ""}});
    auto trail = ctx.agri().trailOf("IT-BATCH-1");
    ASSERT_EQ(trail.size(), 2u);
    EXPECT_EQ(trail[0].stage, "HARVEST");
    EXPECT_EQ(trail[1].stage, "TRANSPORT");

    EXPECT_THROW(ctx.contractEngine().execute(
                     ctx.agri(), core::TransactionType::CONTRACT_AGRI,
                     {{"batchId", "IT-BATCH-1"}, {"stage", "PROCESSING"}, {"actor", "Shady Ltd"},
                      {"location", "nowhere"}, {"details", "backdated"}}),
                 std::invalid_argument);
    ctx.blockchain().minePendingTransactions(ctx.wallet().address());

    // (11) i18n dictionaries are served complete and parallel (I18N-03). Read
    // directly rather than through the /api/i18n/{lang} HTTP route, for the
    // same reason the rest of this test bypasses Drogon (see file header).
    std::ifstream enFile("i18n/messages_en.json");
    std::ifstream plFile("i18n/messages_pl.json");
    ASSERT_TRUE(enFile.good()) << "i18n/messages_en.json must be next to the test binary";
    ASSERT_TRUE(plFile.good()) << "i18n/messages_pl.json must be next to the test binary";
    std::ostringstream enBuf, plBuf;
    enBuf << enFile.rdbuf();
    plBuf << plFile.rdbuf();
    Json::Value en = core::jsonutil::parse(enBuf.str());
    Json::Value pl = core::jsonutil::parse(plBuf.str());
    auto enKeys = en.getMemberNames();
    auto plKeys = pl.getMemberNames();
    std::sort(enKeys.begin(), enKeys.end());
    std::sort(plKeys.begin(), plKeys.end());
    EXPECT_FALSE(enKeys.empty());
    EXPECT_EQ(enKeys, plKeys) << "identical key sets in both languages";
    EXPECT_NE(en["app.title"].asString(), pl["app.title"].asString()) << "values are actually translated";

    // (12) Full chain audit still passes after all activity.
    EXPECT_TRUE(ctx.blockchain().isValid());
}
