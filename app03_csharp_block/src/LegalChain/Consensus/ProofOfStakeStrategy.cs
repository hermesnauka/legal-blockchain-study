using System.Globalization;
using LegalChain.Core;
using LegalChain.Crypto;

namespace LegalChain.Consensus;

/// <summary>
/// Proof of Stake: the right to seal a block is assigned by stake-weighted lottery
/// instead of computational work.
/// </summary>
/// <remarks>
/// <para>The lottery must be <b>deterministic and grinding-resistant</b> so that every node
/// (including a peer receiving the block during sync) selects the same winner: we seed
/// it with the previous block hash — a value the sealer cannot freely re-roll — and walk
/// the cumulative stake distribution. Security intuition: attacking the network requires
/// owning a large share of the very asset the attack would devalue ("skin in the game"),
/// and misbehaviour can be punished by slashing stake. Energy cost is negligible, which
/// is why PoS is the default strategy of this compliance-oriented ledger (ESG reporting
/// is part of regulatory reality).</para>
/// <para>MVP simplification: the validator set is a fixed, documented registry; production
/// systems derive it from on-chain staking transactions.</para>
/// </remarks>
public class ProofOfStakeStrategy : IConsensusStrategy
{
    /// <summary>Demo validator registry: address → staked tokens (insertion order matters for determinism).</summary>
    private static readonly (string Validator, long Stake)[] Stakes =
    [
        ("validator-alpha", 60L),
        ("validator-beta", 30L),
        ("validator-gamma", 10L)
    ];

    public string Name => "POS";

    public string Description
        => "Proof of Stake: deterministic stake-weighted lottery seeded by the previous "
           + "block hash; negligible energy use, security from economic stake.";

    public Block Seal(BlockCandidate candidate)
    {
        var (winner, stake) = SelectValidator(candidate.PreviousHash);
        string proof = "stake-winner=" + winner + ";stake=" + stake
                       + ";seed=" + candidate.PreviousHash[..12];
        return candidate.Seal(Name, proof, 0);
    }

    public bool Validate(Block block)
    {
        // Recompute the lottery from the same seed: the proof must name the same winner.
        var (expectedWinner, _) = SelectValidator(block.PreviousHash);
        return block.Proof.StartsWith("stake-winner=" + expectedWinner + ";", StringComparison.Ordinal)
               && block.HashIsConsistent();
    }

    /// <summary>Deterministic stake-weighted selection seeded by the previous block hash.</summary>
    private static (string Validator, long Stake) SelectValidator(string previousHash)
    {
        long totalStake = Stakes.Sum(s => s.Stake);
        // First 12 hex chars of SHA3(previousHash) → uniform draw in [0, totalStake).
        long draw = long.Parse(HashUtil.Sha3(previousHash)[..12], NumberStyles.HexNumber,
            CultureInfo.InvariantCulture) % totalStake;
        long cumulative = 0;
        foreach (var entry in Stakes)
        {
            cumulative += entry.Stake;
            if (draw < cumulative)
            {
                return entry;
            }
        }
        throw new InvalidOperationException("Stake distribution exhausted — unreachable");
    }
}
