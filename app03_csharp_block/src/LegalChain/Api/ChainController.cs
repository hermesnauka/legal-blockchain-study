using LegalChain.Core;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>The general ledger view: read the chain, audit it, and seal new blocks.</summary>
[ApiController]
[Route("/api/chain")]
public class ChainController : ControllerBase
{
    private readonly Blockchain _blockchain;
    private readonly NodeWallet _wallet;

    public ChainController(Blockchain blockchain, NodeWallet wallet)
    {
        _blockchain = blockchain;
        _wallet = wallet;
    }

    [HttpGet]
    public object Chain()
    {
        IReadOnlyList<Block> blocks = _blockchain.Blocks();
        return new
        {
            length = blocks.Count,
            valid = _blockchain.IsValid(),
            blocks
        };
    }

    /// <summary>Full independent audit: hashes, links, Merkle roots, signatures, consensus proofs.</summary>
    [HttpGet("validate")]
    public object Validate()
    {
        bool valid = _blockchain.IsValid();
        return new
        {
            valid,
            message = valid
                ? "Chain audit passed: all hashes, links, ML-DSA signatures and consensus proofs verified."
                : "Chain audit FAILED: at least one block or signature does not verify."
        };
    }

    /// <summary>Seals pending transactions into a block using the active consensus strategy.</summary>
    [HttpPost("mine")]
    public Block Mine([FromBody] MineRequest? request)
    {
        string validator = !string.IsNullOrWhiteSpace(request?.ValidatorId)
            ? request.ValidatorId
            : _wallet.Address;
        return _blockchain.MinePendingTransactions(validator);
    }

    public record MineRequest(string? ValidatorId);
}
