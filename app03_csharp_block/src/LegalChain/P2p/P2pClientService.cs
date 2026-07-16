using System.Net.WebSockets;

namespace LegalChain.P2p;

/// <summary>
/// Outbound side of the P2P mesh: connects this node to a remote peer's
/// <c>/ws/p2p</c> endpoint. The receive loop runs as a thread-pool task
/// (async/await — the .NET analogue of the Java node's Loom virtual threads:
/// cheap, blocking-style I/O), and the moment the socket opens the PQC handshake
/// starts with this node as initiator.
/// </summary>
public class P2pClientService
{
    private readonly P2pService _p2p;

    public P2pClientService(P2pService p2p)
    {
        _p2p = p2p;
    }

    /// <summary>
    /// Connects to a peer, e.g. <c>ws://localhost:8101/ws/p2p</c>.
    /// The handshake continues asynchronously after this method returns.
    /// </summary>
    public async Task ConnectAsync(string url)
    {
        var socket = new ClientWebSocket();
        try
        {
            using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(10));
            await socket.ConnectAsync(new Uri(url), timeout.Token);
        }
        catch (Exception e)
        {
            socket.Dispose();
            throw new ArgumentException("Cannot connect to peer " + url + ": " + e.Message, e);
        }
        PeerSession peer = await _p2p.RegisterAsync(socket, initiator: true, url);
        _ = Task.Run(() => _p2p.ReceiveLoopAsync(peer));
    }
}
