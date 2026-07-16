using System.ComponentModel.DataAnnotations;
using LegalChain.Core;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>
/// Token transfers. In the MVP the browser has no key store, so the node wallet signs
/// on the user's behalf (custodial model); the ML-DSA signature is created here and
/// verified again by <see cref="Blockchain.Submit"/> — the ledger never trusts the API layer.
/// </summary>
[ApiController]
[Route("/api/transactions")]
public class TransactionController : ControllerBase
{
    private readonly Blockchain _blockchain;
    private readonly NodeWallet _wallet;

    public TransactionController(Blockchain blockchain, NodeWallet wallet)
    {
        _blockchain = blockchain;
        _wallet = wallet;
    }

    [HttpGet("pending")]
    public IReadOnlyList<Transaction> Pending() => _blockchain.PendingTransactions();

    [HttpPost]
    public Transaction Submit([FromBody] TransferRequest request)
    {
        var unsigned = new Transaction(
            Guid.NewGuid().ToString(),
            DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            _wallet.Address,
            request.Recipient,
            request.Amount,
            TransactionType.TRANSFER,
            request.Memo is null
                ? new Dictionary<string, string>()
                : new Dictionary<string, string> { ["memo"] = request.Memo },
            _wallet.PublicKeyBase64,
            null);
        Transaction signed = unsigned with { Signature = _wallet.Sign(unsigned.ContentToSign()) };
        return _blockchain.Submit(signed);
    }

    public record TransferRequest(
        [property: Required(AllowEmptyStrings = false)] string Recipient,
        [property: Range(0, double.MaxValue)] decimal Amount,
        string? Memo);
}
