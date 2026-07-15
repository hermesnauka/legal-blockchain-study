package com.devpowers.legalchain.consensus;

import com.devpowers.legalchain.core.Block;
import com.devpowers.legalchain.core.BlockCandidate;
import com.devpowers.legalchain.crypto.HashUtil;
import org.springframework.stereotype.Component;

import java.util.List;
import java.util.StringJoiner;

/**
 * Byzantine Fault Tolerance (pBFT-style, simulated with an in-process validator panel).
 *
 * <p>The classical result: with {@code n = 3f + 1} validators, agreement is guaranteed
 * even if {@code f} of them are arbitrarily malicious ("Byzantine"), because any two
 * quorums of {@code 2f + 1} votes intersect in at least one honest validator. Here
 * {@code n = 4, f = 1}: a block is sealed only when at least 3 of the 4 panel members
 * approve the candidate's Merkle root. BFT gives <b>immediate finality</b> (no
 * probabilistic confirmations, no forks), which is precisely the property permissioned,
 * regulated ledgers need — a court or auditor can treat an appended entry as final.</p>
 *
 * <p>Votes are simulated deterministically (each validator independently recomputes and
 * signs off on the candidate hash), so peers can re-validate the recorded quorum. In a
 * production network each vote would be an ML-DSA signature from a distinct node.</p>
 */
@Component
public class BftStrategy implements ConsensusStrategy {

    private static final List<String> PANEL =
            List.of("validator-1", "validator-2", "validator-3", "validator-4");
    private static final int QUORUM = 3; // 2f + 1 with f = 1

    @Override
    public String name() {
        return "BFT";
    }

    @Override
    public String description() {
        return "Byzantine Fault Tolerance (pBFT): 4 validators, quorum of 3 (tolerates 1 "
                + "malicious node); immediate finality, no forks.";
    }

    @Override
    public Block seal(BlockCandidate candidate) {
        String subject = candidate.merkleRoot() + "|" + candidate.previousHash();
        StringJoiner votes = new StringJoiner(",");
        int approvals = 0;
        for (String validator : PANEL) {
            // Deterministic simulated vote: validator's attestation over the candidate.
            String attestation = HashUtil.sha3(validator + "|" + subject).substring(0, 8);
            votes.add(validator + ":APPROVE:" + attestation);
            approvals++;
            if (approvals == PANEL.size()) {
                break;
            }
        }
        if (approvals < QUORUM) {
            throw new IllegalStateException("BFT quorum not reached");
        }
        return candidate.seal(name(), "quorum=" + approvals + "/" + PANEL.size() + ";votes=" + votes, 0);
    }

    @Override
    public boolean validate(Block block) {
        if (!block.hashIsConsistent() || !block.proof().startsWith("quorum=")) {
            return false;
        }
        // Re-verify each recorded attestation against the block's own commitments.
        String subject = block.merkleRoot() + "|" + block.previousHash();
        String votesPart = block.proof().substring(block.proof().indexOf(";votes=") + 7);
        int validVotes = 0;
        for (String vote : votesPart.split(",")) {
            String[] parts = vote.split(":");
            if (parts.length == 3 && parts[1].equals("APPROVE")
                    && parts[2].equals(HashUtil.sha3(parts[0] + "|" + subject).substring(0, 8))) {
                validVotes++;
            }
        }
        return validVotes >= QUORUM;
    }
}
