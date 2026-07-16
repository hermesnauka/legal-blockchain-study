using System.Globalization;

namespace LegalChain.Core;

/// <summary>
/// Immutable ledger transaction (C# record — immutability is a compliance feature:
/// once created, an entry cannot be mutated in memory, only superseded on-chain).
/// </summary>
/// <remarks>
/// Every non-reward transaction is signed with the sender's ML-DSA key over
/// <see cref="ContentToSign"/>, and carries the sender's public key so that any node or
/// external auditor can verify authorship offline. The sender "address" is a SHA3-256
/// fingerprint of that key — pseudonymous, self-sovereign identity.
/// </remarks>
/// <param name="Id">unique transaction id (UUID)</param>
/// <param name="Timestamp">epoch millis at creation</param>
/// <param name="Sender">sender address (key fingerprint) or <see cref="System"/> for rewards</param>
/// <param name="Recipient">recipient address</param>
/// <param name="Amount">token amount (zero for pure data transactions)</param>
/// <param name="Type">ledger entry category</param>
/// <param name="Payload">type-specific data (NFT metadata, contract arguments, memo)</param>
/// <param name="SenderPublicKey">Base64 ML-DSA public key (<c>null</c> for REWARD)</param>
/// <param name="Signature">Base64 ML-DSA signature over <see cref="ContentToSign"/></param>
public record Transaction(
    string Id,
    long Timestamp,
    string Sender,
    string Recipient,
    decimal Amount,
    TransactionType Type,
    IReadOnlyDictionary<string, string>? Payload,
    string? SenderPublicKey,
    string? Signature)
{
    public const string System = "SYSTEM";

    /// <summary>
    /// Canonical string covered by the ML-DSA signature. The payload map is serialized
    /// in sorted-key order so signer and verifier always derive the same bytes.
    /// </summary>
    public string ContentToSign()
    {
        var sorted = (Payload ?? new Dictionary<string, string>())
            .OrderBy(e => e.Key, StringComparer.Ordinal)
            .Select(e => e.Key + "=" + e.Value);
        return Id + '|' + Timestamp + '|' + Sender + '|' + Recipient + '|'
               + FormatAmount(Amount) + '|' + Type + '|'
               + "{" + string.Join(", ", sorted) + "}";
    }

    public bool IsReward() => Type == TransactionType.REWARD;

    /// <summary>Canonical plain decimal rendering without trailing zeros (BigDecimal-style).</summary>
    public static string FormatAmount(decimal amount)
    {
        string s = amount.ToString(CultureInfo.InvariantCulture);
        return s.Contains('.') ? s.TrimEnd('0').TrimEnd('.') : s;
    }
}
