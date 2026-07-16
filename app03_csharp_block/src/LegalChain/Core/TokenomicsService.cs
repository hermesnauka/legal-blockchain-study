using LegalChain.Config;

namespace LegalChain.Core;

/// <summary>
/// Educational tokenomics: a fixed block reward that halves every N blocks under a hard
/// supply cap (the Bitcoin issuance model, scaled down so halvings are observable in a
/// classroom session).
/// </summary>
/// <remarks>
/// Why this matters for compliance: the total supply and the issuance schedule are
/// deterministic functions of the chain itself — no discretionary "printing". An auditor
/// can recompute the entire monetary base from the ledger, which is exactly the
/// transparency property regulators (e.g. under MiCA) expect from a token issuer.
/// </remarks>
public class TokenomicsService
{
    private readonly decimal _blockReward;
    private readonly int _halvingInterval;
    private readonly decimal _maxSupply;

    public TokenomicsService(LegalChainOptions options)
    {
        _blockReward = options.Tokenomics.BlockReward;
        _halvingInterval = options.Tokenomics.HalvingInterval;
        _maxSupply = options.Tokenomics.MaxSupply;
    }

    /// <summary>Reward for the block at <paramref name="blockIndex"/>, after halvings and the supply cap.</summary>
    public decimal RewardForBlock(long blockIndex, decimal supplyAlreadyIssued)
    {
        int halvings = (int)(blockIndex / _halvingInterval);
        decimal reward = _blockReward;
        for (int i = 0; i < halvings; i++)
        {
            reward /= 2m;
        }
        decimal remaining = _maxSupply - supplyAlreadyIssued;
        return Math.Max(Math.Min(reward, remaining), 0m);
    }

    /// <summary>Coinbase-style reward transaction crediting the sealing validator.</summary>
    public Transaction RewardTransaction(string validatorAddress, long blockIndex,
        decimal supplyAlreadyIssued)
        => new(
            Guid.NewGuid().ToString(),
            DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            Transaction.System,
            validatorAddress,
            RewardForBlock(blockIndex, supplyAlreadyIssued),
            TransactionType.REWARD,
            new Dictionary<string, string>
            {
                ["reason"] = "block-reward",
                ["block"] = blockIndex.ToString()
            },
            null,
            null);

    /// <summary>Total supply issued so far = sum of all REWARD transactions in the chain.</summary>
    public decimal IssuedSupply(IReadOnlyList<Block> chain)
        => chain.SelectMany(b => b.Transactions)
            .Where(t => t.IsReward())
            .Sum(t => t.Amount);
}
