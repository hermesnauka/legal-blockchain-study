using LegalChain.Core;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>This node's post-quantum wallet (self-sovereign identity) and the derived balances.</summary>
[ApiController]
[Route("/api/wallet")]
public class WalletController : ControllerBase
{
    private readonly NodeWallet _wallet;
    private readonly Blockchain _blockchain;

    public WalletController(NodeWallet wallet, Blockchain blockchain)
    {
        _wallet = wallet;
        _blockchain = blockchain;
    }

    [HttpGet]
    public object Wallet() => new
    {
        address = _wallet.Address,
        algorithm = _wallet.Algorithm,
        publicKey = _wallet.PublicKeyBase64,
        fingerprint = _wallet.Address,
        balance = _blockchain.Balances().GetValueOrDefault(_wallet.Address, 0m)
    };

    /// <summary>All balances, derived by replaying the ledger — never stored, always auditable.</summary>
    [HttpGet("balances")]
    public Dictionary<string, decimal> Balances() => _blockchain.Balances();
}
