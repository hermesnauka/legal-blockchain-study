// Each strategy must seal a candidate and re-validate its own proof
// deterministically. Mirrors app01_java_block's ConsensusStrategyTest.java.
#include <gtest/gtest.h>

#include "consensus/BftStrategy.h"
#include "consensus/ProofOfStakeStrategy.h"
#include "consensus/ProofOfWorkStrategy.h"
#include "core/BlockCandidate.h"

using namespace legalchain;

namespace {
core::BlockCandidate candidate() {
    return core::BlockCandidate(1, 1700000000000LL, std::string(64, 'a'), {}, "validator-x");
}
} // namespace

TEST(ConsensusStrategyTest, ProofOfWorkMeetsDifficultyAndValidates) {
    consensus::ProofOfWorkStrategy pow;
    core::Block block = pow.seal(candidate());

    EXPECT_EQ(block.hash().compare(0, 4, consensus::ProofOfWorkStrategy::DIFFICULTY_PREFIX), 0);
    EXPECT_TRUE(pow.validate(block));
}

TEST(ConsensusStrategyTest, ProofOfStakeSelectionIsDeterministic) {
    consensus::ProofOfStakeStrategy pos;
    core::Block first = pos.seal(candidate());
    core::Block second = pos.seal(candidate());

    EXPECT_EQ(first.proof(), second.proof()); // same seed, same winner
    EXPECT_TRUE(pos.validate(first));
}

TEST(ConsensusStrategyTest, BftRecordsVerifiableQuorum) {
    consensus::BftStrategy bft;
    core::Block block = bft.seal(candidate());

    EXPECT_EQ(block.proof().compare(0, 10, "quorum=4/4"), 0);
    EXPECT_TRUE(bft.validate(block));
}

TEST(ConsensusStrategyTest, StrategiesRejectForeignProofs) {
    consensus::ProofOfStakeStrategy pos;
    core::Block posBlock = pos.seal(candidate());

    consensus::BftStrategy bft;
    EXPECT_FALSE(bft.validate(posBlock));
}
