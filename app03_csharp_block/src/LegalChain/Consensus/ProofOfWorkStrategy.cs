using LegalChain.Core;

namespace LegalChain.Consensus;

/// <summary>
/// Proof of Work: the sealer increments a nonce until the block hash falls below a
/// difficulty target (here: a required prefix of zero hex digits).
/// </summary>
/// <remarks>
/// Security intuition: rewriting history requires redoing the accumulated work of
/// every later block faster than the honest network extends it — economically
/// infeasible past a few confirmations. The proof is trivially verifiable (one hash)
/// but expensive to produce, an asymmetry that needs no identities at all.
/// Trade-off: enormous energy cost, which is why this MVP defaults to PoS and keeps
/// difficulty low (4 hex zeros ≈ 65k hashes) for classroom responsiveness.
/// </remarks>
public class ProofOfWorkStrategy : IConsensusStrategy
{
    internal const string DifficultyPrefix = "0000";

    public string Name => "POW";

    public string Description
        => "Proof of Work: nonce search until the SHA3-256 block hash starts with "
           + DifficultyPrefix + "; costly to produce, one hash to verify.";

    public Block Seal(BlockCandidate candidate)
    {
        long nonce = 0;
        while (true)
        {
            Block block = candidate.Seal(Name, "difficulty=" + DifficultyPrefix.Length, nonce);
            if (block.Hash.StartsWith(DifficultyPrefix, StringComparison.Ordinal))
            {
                return block;
            }
            nonce++;
        }
    }

    public bool Validate(Block block)
        => block.Hash.StartsWith(DifficultyPrefix, StringComparison.Ordinal) && block.HashIsConsistent();
}
