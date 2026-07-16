namespace LegalChain.Core;

/// <summary>Ledger entry categories; contract types make sector use-cases auditable on-chain.</summary>
/// <remarks>
/// Member names are deliberately UPPER_SNAKE_CASE: they are serialized verbatim to JSON
/// (API contract v1 shared with the Java and C++ nodes and the React frontend).
/// </remarks>
public enum TransactionType
{
    TRANSFER,
    REWARD,
    NFT_MINT,
    CONTRACT_MEDICAL,
    CONTRACT_AGRI
}
