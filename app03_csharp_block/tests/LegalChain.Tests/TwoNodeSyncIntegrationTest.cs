using LegalChain.Core;
using LegalChain.P2p;
using Microsoft.AspNetCore.Builder;
using Microsoft.Extensions.DependencyInjection;

namespace LegalChain.Tests;

/// <summary>
/// Automated version of the two-node demo from the Definition of Done: two full
/// ASP.NET Core nodes (real Kestrel, real WebSockets) in one process, connected with the
/// real ML-DSA + ML-KEM handshake, converging on one ledger.
/// </summary>
/// <remarks>
/// What this proves end-to-end: distinct self-sovereign node identities (BE-14),
/// a deterministic shared genesis (BE-02), a PQC-secured channel established without
/// any pre-shared secret (SEC-04), and full-chain adoption where the receiving node
/// re-audits every block, signature and proof locally before accepting (E4).
/// </remarks>
public class TwoNodeSyncIntegrationTest
{
    [Fact]
    public async Task TwoRemoteNodesEstablishPqcChannelAndConvergeOnOneLedger()
    {
        WebApplication nodeA = LegalChainApp.Build(
            ["--urls", "http://127.0.0.1:0", "--node-name", "sync-itest-A"]);
        WebApplication nodeB = LegalChainApp.Build(
            ["--urls", "http://127.0.0.1:0", "--node-name", "sync-itest-B"]);
        await nodeA.StartAsync();
        await nodeB.StartAsync();
        try
        {
            string urlA = nodeA.Urls.Single(); // ephemeral port resolved after StartAsync
            var chainA = nodeA.Services.GetRequiredService<Blockchain>();
            var chainB = nodeB.Services.GetRequiredService<Blockchain>();
            var walletA = nodeA.Services.GetRequiredService<NodeWallet>();
            var walletB = nodeB.Services.GetRequiredService<NodeWallet>();
            var p2pA = nodeA.Services.GetRequiredService<P2pService>();
            var p2pB = nodeB.Services.GetRequiredService<P2pService>();

            // BE-14: each node generates its own ML-DSA identity — fingerprints differ.
            Assert.NotEqual(walletA.Address, walletB.Address);
            // BE-02: yet both derive the identical deterministic genesis block.
            Assert.Equal(chainA.Blocks()[0].Hash, chainB.Blocks()[0].Hash);

            // Grow node A's ledger past node B's.
            chainA.MinePendingTransactions(walletA.Address);
            chainA.MinePendingTransactions(walletA.Address);
            int targetLength = chainA.Length;

            // B dials A: WebSocket + ML-DSA-signed hello + ML-KEM encapsulation.
            string wsUrl = urlA.Replace("http://", "ws://") + "/ws/p2p";
            await nodeB.Services.GetRequiredService<P2pClientService>().ConnectAsync(wsUrl);
            await AwaitAsync("PQC handshake on both sides",
                () => p2pB.ConnectedPeers().Count > 0 && p2pA.ConnectedPeers().Count > 0);

            // Request the ledger over the encrypted channel; B re-audits before adopting.
            await p2pB.SyncWithPeersAsync();
            await AwaitAsync("chain convergence",
                () => chainB.Length >= targetLength
                      && chainB.Blocks()[^1].Hash == chainA.Blocks()[^1].Hash);

            Assert.True(chainB.IsValid(), "adopted chain passes B's own full audit");
            Assert.Equal(chainA.Blocks()[^1].Hash, chainB.Blocks()[^1].Hash);
        }
        finally
        {
            await nodeA.StopAsync();
            await nodeB.StopAsync();
        }
    }

    /// <summary>Polls the condition for up to 20 s — handshake and sync are asynchronous.</summary>
    private static async Task AwaitAsync(string what, Func<bool> condition)
    {
        var deadline = DateTime.UtcNow.AddSeconds(20);
        while (DateTime.UtcNow < deadline)
        {
            if (condition())
            {
                return;
            }
            await Task.Delay(200);
        }
        Assert.Fail("timed out after 20s waiting for " + what);
    }
}
