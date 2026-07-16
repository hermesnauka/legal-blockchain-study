using LegalChain.Consensus;
using LegalChain.Core;

namespace LegalChain.Tests;

/// <summary>
/// BE-08 / BE-09: every executable strategy seals a candidate into a block that it can
/// deterministically re-validate (peers re-check proofs during sync without trusting the
/// sender), and the engine hot-swaps strategies by name.
/// </summary>
public class ConsensusStrategyTest
{
    private static BlockCandidate Candidate() => new(
        1, 1700000000000, new string('0', 64), [], "validator-test");

    [Fact]
    public void ProofOfWorkFindsDifficultyPrefixAndRevalidates()
    {
        var pow = new ProofOfWorkStrategy();
        Block block = pow.Seal(Candidate());
        Assert.StartsWith("0000", block.Hash);
        Assert.True(pow.Validate(block));
        Assert.False(pow.Validate(block with { Proof = block.Proof + "x" }),
            "any header change breaks the sealed hash");
    }

    [Fact]
    public void ProofOfStakeLotteryIsDeterministicPerSeed()
    {
        var pos = new ProofOfStakeStrategy();
        Block first = pos.Seal(Candidate());
        Block second = pos.Seal(Candidate());
        Assert.Equal(first.Proof, second.Proof); // same previousHash → same winner
        Assert.StartsWith("stake-winner=", first.Proof);
        Assert.True(pos.Validate(first));

        // A proof naming a different winner than the seed dictates must fail.
        Block forged = Candidate().Seal(pos.Name, "stake-winner=validator-nobody;stake=99;seed=x", 0);
        Assert.False(pos.Validate(forged));
    }

    [Fact]
    public void BftRecordsAVerifiableQuorumOfAttestations()
    {
        var bft = new BftStrategy();
        Block block = bft.Seal(Candidate());
        Assert.StartsWith("quorum=4/4", block.Proof);
        Assert.True(bft.Validate(block));

        // Corrupt one attestation: 3/4 still meets the 2f+1 quorum...
        string oneBadVote = block.Proof.Replace("validator-1:APPROVE:", "validator-1:APPROVE:dead0000");
        Assert.True(bft.Validate(Candidate().Seal(bft.Name, oneBadVote, 0)));
        // ...but a vote set below 2f+1 must be rejected (BE-09).
        string votes = block.Proof[(block.Proof.IndexOf(";votes=", StringComparison.Ordinal) + 7)..];
        string twoVotes = string.Join(',', votes.Split(',').Take(2));
        Assert.False(bft.Validate(Candidate().Seal(bft.Name, "quorum=2/4;votes=" + twoVotes, 0)));
    }

    [Fact]
    public void EngineHotSwapsByNameAndRejectsUnknownStrategies()
    {
        var engine = new ConsensusEngine(
            [new ProofOfWorkStrategy(), new ProofOfStakeStrategy(), new BftStrategy()], "POS");
        Assert.Equal("POS", engine.Active.Name);

        engine.SwitchTo("bft"); // case-insensitive, like the REST endpoint
        Assert.Equal("BFT", engine.Active.Name);

        Assert.Equal(["BFT", "POS", "POW"], engine.Available().Select(s => s.Name).ToList());
        Assert.Throws<ArgumentException>(() => engine.SwitchTo("POH"));
        Assert.Throws<ArgumentException>(() => engine.ByName("DPOS"));
    }
}
