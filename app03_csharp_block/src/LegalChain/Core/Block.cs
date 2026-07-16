using LegalChain.Crypto;

namespace LegalChain.Core;

/// <summary>
/// Immutable block of the general ledger (C# record).
/// </summary>
/// <remarks>
/// The block hash covers the index, timestamp, previous hash, Merkle root, validator,
/// consensus proof and nonce. Because <c>previousHash</c> of block N+1 is the hash of
/// block N, retroactively editing <i>any</i> historical transaction invalidates every
/// later block — this hash chaining is the core tamper-evidence property that makes a
/// blockchain acceptable as a compliant, auditable registry.
/// </remarks>
/// <param name="Index">position in the chain (genesis = 0)</param>
/// <param name="Timestamp">epoch millis when the block was sealed</param>
/// <param name="PreviousHash">hash of the preceding block (64 zeros for genesis)</param>
/// <param name="MerkleRoot">Merkle root committing to all transactions in this block</param>
/// <param name="Transactions">the transactions recorded in this block</param>
/// <param name="ValidatorId">address of the node that sealed the block (receives the reward)</param>
/// <param name="ConsensusType">name of the consensus strategy that sealed this block</param>
/// <param name="Proof">strategy-specific proof (PoW target, PoS selection, BFT votes)</param>
/// <param name="Nonce">PoW counter (0 for non-PoW strategies)</param>
/// <param name="Hash">SHA3-256 hash of the block header</param>
public record Block(
    long Index,
    long Timestamp,
    string PreviousHash,
    string MerkleRoot,
    IReadOnlyList<Transaction> Transactions,
    string ValidatorId,
    string ConsensusType,
    string Proof,
    long Nonce,
    string Hash)
{
    /// <summary>Recomputes the header hash; used both when sealing and when auditing the chain.</summary>
    public static string ComputeHash(long index, long timestamp, string previousHash,
        string merkleRoot, string validatorId, string consensusType, string proof, long nonce)
        => HashUtil.Sha3(index + "|" + timestamp + "|" + previousHash + "|" + merkleRoot
                         + "|" + validatorId + "|" + consensusType + "|" + proof + "|" + nonce);

    public static string MerkleRootOf(IReadOnlyList<Transaction> transactions)
        => HashUtil.MerkleRoot(transactions.Select(t => t.ContentToSign()).ToList());

    /// <summary>True if the stored hash matches a recomputation of the header — tamper check.</summary>
    public bool HashIsConsistent()
        => Hash == ComputeHash(Index, Timestamp, PreviousHash,
            MerkleRootOf(Transactions), ValidatorId, ConsensusType, Proof, Nonce);
}
