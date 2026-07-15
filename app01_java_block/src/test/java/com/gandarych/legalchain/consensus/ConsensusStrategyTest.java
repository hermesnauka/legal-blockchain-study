package com.gandarych.legalchain.consensus;

import com.gandarych.legalchain.core.Block;
import com.gandarych.legalchain.core.BlockCandidate;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.assertj.core.api.Assertions.assertThat;

/** Each strategy must seal a candidate and re-validate its own proof deterministically. */
class ConsensusStrategyTest {

    private final BlockCandidate candidate = new BlockCandidate(
            1, 1_700_000_000_000L, "a".repeat(64), List.of(), "validator-x");

    @Test
    void proofOfWorkMeetsDifficultyAndValidates() {
        ProofOfWorkStrategy pow = new ProofOfWorkStrategy();
        Block block = pow.seal(candidate);

        assertThat(block.hash()).startsWith(ProofOfWorkStrategy.DIFFICULTY_PREFIX);
        assertThat(pow.validate(block)).isTrue();
    }

    @Test
    void proofOfStakeSelectionIsDeterministic() {
        ProofOfStakeStrategy pos = new ProofOfStakeStrategy();
        Block first = pos.seal(candidate);
        Block second = pos.seal(candidate);

        assertThat(first.proof()).isEqualTo(second.proof()); // same seed, same winner
        assertThat(pos.validate(first)).isTrue();
    }

    @Test
    void bftRecordsVerifiableQuorum() {
        BftStrategy bft = new BftStrategy();
        Block block = bft.seal(candidate);

        assertThat(block.proof()).startsWith("quorum=4/4");
        assertThat(bft.validate(block)).isTrue();
    }

    @Test
    void strategiesRejectForeignProofs() {
        ProofOfStakeStrategy pos = new ProofOfStakeStrategy();
        Block posBlock = pos.seal(candidate);

        assertThat(new BftStrategy().validate(posBlock)).isFalse();
    }
}
