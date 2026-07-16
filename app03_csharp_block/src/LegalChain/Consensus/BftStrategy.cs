using System.Text;
using LegalChain.Core;
using LegalChain.Crypto;

namespace LegalChain.Consensus;

/// <summary>
/// Byzantine Fault Tolerance (pBFT-style, simulated with an in-process validator panel).
/// </summary>
/// <remarks>
/// <para>The classical result: with <c>n = 3f + 1</c> validators, agreement is guaranteed
/// even if <c>f</c> of them are arbitrarily malicious ("Byzantine"), because any two
/// quorums of <c>2f + 1</c> votes intersect in at least one honest validator. Here
/// <c>n = 4, f = 1</c>: a block is sealed only when at least 3 of the 4 panel members
/// approve the candidate's Merkle root. BFT gives <b>immediate finality</b> (no
/// probabilistic confirmations, no forks), which is precisely the property permissioned,
/// regulated ledgers need — a court or auditor can treat an appended entry as final.</para>
/// <para>Votes are simulated deterministically (each validator independently recomputes and
/// signs off on the candidate hash), so peers can re-validate the recorded quorum. In a
/// production network each vote would be an ML-DSA signature from a distinct node.</para>
/// </remarks>
public class BftStrategy : IConsensusStrategy
{
    private static readonly string[] Panel =
        ["validator-1", "validator-2", "validator-3", "validator-4"];

    private const int Quorum = 3; // 2f + 1 with f = 1

    public string Name => "BFT";

    public string Description
        => "Byzantine Fault Tolerance (pBFT): 4 validators, quorum of 3 (tolerates 1 "
           + "malicious node); immediate finality, no forks.";

    public Block Seal(BlockCandidate candidate)
    {
        string subject = candidate.MerkleRoot() + "|" + candidate.PreviousHash;
        var votes = new StringBuilder();
        int approvals = 0;
        foreach (string validator in Panel)
        {
            // Deterministic simulated vote: validator's attestation over the candidate.
            string attestation = HashUtil.Sha3(validator + "|" + subject)[..8];
            if (votes.Length > 0)
            {
                votes.Append(',');
            }
            votes.Append(validator).Append(":APPROVE:").Append(attestation);
            approvals++;
        }
        if (approvals < Quorum)
        {
            throw new InvalidOperationException("BFT quorum not reached");
        }
        return candidate.Seal(Name, "quorum=" + approvals + "/" + Panel.Length + ";votes=" + votes, 0);
    }

    public bool Validate(Block block)
    {
        if (!block.HashIsConsistent() || !block.Proof.StartsWith("quorum=", StringComparison.Ordinal))
        {
            return false;
        }
        // Re-verify each recorded attestation against the block's own commitments.
        string subject = block.MerkleRoot + "|" + block.PreviousHash;
        string votesPart = block.Proof[(block.Proof.IndexOf(";votes=", StringComparison.Ordinal) + 7)..];
        int validVotes = 0;
        foreach (string vote in votesPart.Split(','))
        {
            string[] parts = vote.Split(':');
            if (parts.Length == 3 && parts[1] == "APPROVE"
                && parts[2] == HashUtil.Sha3(parts[0] + "|" + subject)[..8])
            {
                validVotes++;
            }
        }
        return validVotes >= Quorum;
    }
}
