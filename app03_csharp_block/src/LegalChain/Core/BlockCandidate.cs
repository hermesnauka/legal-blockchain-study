namespace LegalChain.Core;

/// <summary>
/// An unsealed block handed to a <see cref="LegalChain.Consensus.IConsensusStrategy"/>,
/// which produces the proof/nonce and the final <see cref="Block"/>. Separating the
/// candidate from the sealed block is what lets consensus algorithms be swapped
/// (Strategy pattern) without touching the ledger core.
/// </summary>
public record BlockCandidate(
    long Index,
    long Timestamp,
    string PreviousHash,
    IReadOnlyList<Transaction> Transactions,
    string ValidatorId)
{
    public string MerkleRoot() => Block.MerkleRootOf(Transactions);

    /// <summary>Convenience for strategies: seal this candidate with the given proof and nonce.</summary>
    public Block Seal(string consensusType, string proof, long nonce)
    {
        string merkleRoot = MerkleRoot();
        string hash = Block.ComputeHash(Index, Timestamp, PreviousHash, merkleRoot,
            ValidatorId, consensusType, proof, nonce);
        return new Block(Index, Timestamp, PreviousHash, merkleRoot, Transactions,
            ValidatorId, consensusType, proof, nonce, hash);
    }
}
