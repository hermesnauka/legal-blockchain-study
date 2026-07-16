namespace LegalChain.Contracts;

/// <summary>
/// Blockchain 3.0 smart contract abstraction: deterministic business logic whose every
/// execution is recorded as a signed ledger transaction.
/// </summary>
/// <remarks>
/// Design rule for compliance (GDPR art. 17 "right to erasure" vs. immutability):
/// contracts must keep <b>personal data off-chain</b> and record on-chain only hashes,
/// pseudonymous identifiers and consent/state transitions. The chain then proves
/// <i>that</i> and <i>when</i> something happened without ever containing erasable
/// personal data — the standard pattern that reconciles blockchains with GDPR.
/// (Named <c>SmartContract</c> in the Java and C++ nodes; the leading I is C# idiom.)
/// </remarks>
public interface ISmartContract
{
    /// <summary>Stable contract identifier recorded in the transaction payload.</summary>
    string ContractId { get; }

    /// <summary>Sector / purpose description surfaced by the educational frontend.</summary>
    string Description { get; }

    /// <summary>
    /// Validates the input against the contract's rules and returns the state transition
    /// to be recorded on-chain. Implementations must be deterministic and side-effect
    /// free apart from their own state registry (same input → same result on every node).
    /// </summary>
    /// <exception cref="ArgumentException">if the input violates contract rules</exception>
    ContractResult Execute(IReadOnlyDictionary<string, string> input);
}
