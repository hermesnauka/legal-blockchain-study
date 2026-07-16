using System.ComponentModel.DataAnnotations;
using LegalChain.Config;
using LegalChain.Consensus;
using LegalChain.Core;
using LegalChain.P2p;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>Node identity and P2P operations (connect to a peer, trigger a full ledger sync).</summary>
[ApiController]
public class NodeController : ControllerBase
{
    private readonly NodeWallet _wallet;
    private readonly Blockchain _blockchain;
    private readonly ConsensusEngine _consensus;
    private readonly P2pService _p2p;
    private readonly P2pClientService _p2pClient;
    private readonly LegalChainOptions _options;

    public NodeController(NodeWallet wallet, Blockchain blockchain, ConsensusEngine consensus,
        P2pService p2p, P2pClientService p2pClient, LegalChainOptions options)
    {
        _wallet = wallet;
        _blockchain = blockchain;
        _consensus = consensus;
        _p2p = p2p;
        _p2pClient = p2pClient;
        _options = options;
    }

    [HttpGet("/api/node")]
    public object Node() => new
    {
        nodeId = _wallet.Address,
        name = _options.NodeName,
        port = _options.Port,
        consensus = _consensus.Active.Name,
        peers = _p2p.ConnectedPeers(),
        chainLength = _blockchain.Length
    };

    /// <summary>Opens a PQC-secured link to a remote peer, e.g. <c>ws://localhost:8101/ws/p2p</c>.</summary>
    [HttpPost("/api/p2p/connect")]
    public async Task<object> Connect([FromBody] ConnectRequest request)
    {
        await _p2pClient.ConnectAsync(request.Url);
        return new { connected = true, peer = request.Url };
    }

    /// <summary>Requests the ledger from all established peers; longest valid chain wins locally.</summary>
    [HttpPost("/api/p2p/sync")]
    public async Task<object> Sync()
    {
        int asked = await _p2p.SyncWithPeersAsync();
        return new
        {
            result = asked > 0
                ? "Sync requested from " + asked + " peer(s)"
                : "No established peers to sync with",
            chainLength = _blockchain.Length
        };
    }

    public record ConnectRequest([Required(AllowEmptyStrings = false)] string Url);
}
