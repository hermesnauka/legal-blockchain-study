using LegalChain.Core;

namespace LegalChain.Consensus;

/// <summary>
/// Strategy pattern for consensus: the ledger core is agnostic about <i>how</i> a block
/// earns the right to be appended. Each implementation seals a candidate (producing its
/// proof) and can independently re-validate any sealed block — validation must be a pure
/// function of the block, because remote nodes re-check proofs during P2P sync without
/// trusting the sender.
/// </summary>
/// <remarks>
/// This is the "Consensus and Scalability" seam of the architecture: adding DPoS,
/// Proof-of-Reputation, PoH or PoET later means adding one class, not touching the core.
/// (Named <c>ConsensusStrategy</c> in the Java and C++ nodes; the leading I is C# idiom.)
/// </remarks>
public interface IConsensusStrategy
{
    /// <summary>Stable identifier used in the API and stored in each block (<c>consensusType</c>).</summary>
    string Name { get; }

    /// <summary>One-line explanation surfaced by the educational frontend.</summary>
    string Description { get; }

    /// <summary>Produces the proof for the candidate and returns the sealed, hashed block.</summary>
    Block Seal(BlockCandidate candidate);

    /// <summary>Re-verifies the proof of a sealed block (used when auditing chains from peers).</summary>
    bool Validate(Block block);
}
