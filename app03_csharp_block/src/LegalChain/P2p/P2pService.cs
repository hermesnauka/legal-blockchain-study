using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text;
using System.Text.Json;
using LegalChain.Core;
using LegalChain.Crypto;

namespace LegalChain.P2p;

/// <summary>
/// The P2P ledger-synchronization protocol between two remote nodes, secured end-to-end
/// with post-quantum cryptography.
/// </summary>
/// <remarks>
/// <para><b>Handshake (QKD-inspired, MITM-resistant)</b></para>
/// <list type="number">
///   <item><b>HELLO</b> (initiator → responder): the initiator's node id, its ML-DSA public
///       key, a <i>fresh ephemeral</i> ML-KEM public key, and an ML-DSA signature over
///       <c>kemPublicKey|nodeId</c>.</item>
///   <item><b>ENCAPS</b> (responder → initiator): the responder encapsulates a random
///       32-byte secret against the initiator's KEM key and returns the ciphertext,
///       signed with its own ML-DSA key over <c>ciphertext|nodeId</c>.</item>
///   <item>Both sides now hold the same one-time AES-256-GCM session key; every subsequent
///       frame is a <b>SECURE</b> message (authenticated encryption).</item>
/// </list>
/// <para><b>Why a man-in-the-middle fails.</b> The classic MITM move is substituting key
/// material in flight. Here both handshake messages are signed with ML-DSA, and each side
/// checks that the claimed node id equals the SHA3-256 fingerprint of the signing key. An
/// attacker who swaps the KEM key or the ciphertext must forge an ML-DSA signature —
/// believed infeasible even for a quantum adversary (lattice hardness, NIST FIPS 204).
/// Tampering with SECURE frames fails the GCM authentication tag instead.</para>
/// <para><b>Trust without revealing identity (ZKP-style pseudonymity).</b> Note what the
/// handshake does <i>not</i> contain: names, certificates, IP-bound identities. Each party
/// proves exactly one statement — "I control the private key behind this fingerprint" — by
/// producing a valid signature, and nothing else. This is the zero-knowledge-flavoured
/// privacy model of self-sovereign identity: trust is established between pseudonyms, and
/// even the adopted ledger data is never trusted on channel security alone —
/// <see cref="Blockchain.ReplaceChain"/> re-audits every block, every signature and every
/// consensus proof locally before accepting a peer's chain.</para>
/// </remarks>
public class P2pService
{
    /// <summary>Chain frames carry ~7 KB per signed transaction (ML-DSA sizes) — allow large messages.</summary>
    public const int MaxMessageBytes = 10 * 1024 * 1024;

    private readonly Blockchain _blockchain;
    private readonly NodeWallet _wallet;
    private readonly PqcSignatureService _signatures;
    private readonly PqcKeyExchangeService _keyExchange;
    private readonly SecureChannel _channel;
    private readonly EventBus _events;
    private readonly ILogger<P2pService> _log;

    /// <summary>Live peer links keyed by session id.</summary>
    private readonly ConcurrentDictionary<string, PeerSession> _peers = new();

    public P2pService(Blockchain blockchain, NodeWallet wallet, PqcSignatureService signatures,
        PqcKeyExchangeService keyExchange, SecureChannel channel, EventBus events,
        ILogger<P2pService> log)
    {
        _blockchain = blockchain;
        _wallet = wallet;
        _signatures = signatures;
        _keyExchange = keyExchange;
        _channel = channel;
        _events = events;
        _log = log;
        // Gossip: whenever this node seals a block, tell peers so they can pull the longer chain.
        events.Subscribe(OnLedgerEvent);
    }

    // ------------------------------------------------------------------ lifecycle

    public async Task<PeerSession> RegisterAsync(WebSocket socket, bool initiator, string? url)
    {
        var peer = new PeerSession(socket, initiator) { Url = url };
        _peers[peer.Id] = peer;
        if (initiator)
        {
            await SendHelloAsync(peer);
        }
        return peer;
    }

    public void Unregister(PeerSession peer) => _peers.TryRemove(peer.Id, out _);

    public IReadOnlyList<Dictionary<string, string>> ConnectedPeers()
        => _peers.Values
            .Where(p => p.Established)
            .Select(p => new Dictionary<string, string>
            {
                ["nodeId"] = p.PeerNodeId ?? "?",
                ["url"] = p.Url ?? "(inbound)"
            })
            .ToList();

    /// <summary>
    /// Reads text frames from the socket until it closes, feeding each into the protocol
    /// state machine. Runs on the async thread pool — the .NET analogue of the Java
    /// node's Loom virtual threads: blocking-style I/O code, negligible thread cost.
    /// </summary>
    public async Task ReceiveLoopAsync(PeerSession peer, CancellationToken ct = default)
    {
        var buffer = new byte[64 * 1024];
        var message = new MemoryStream();
        try
        {
            while (peer.Socket.State == WebSocketState.Open && !ct.IsCancellationRequested)
            {
                WebSocketReceiveResult result = await peer.Socket.ReceiveAsync(buffer, ct);
                if (result.MessageType == WebSocketMessageType.Close)
                {
                    break;
                }
                message.Write(buffer, 0, result.Count);
                if (message.Length > MaxMessageBytes)
                {
                    throw new InvalidOperationException("P2P frame exceeds size limit");
                }
                if (result.EndOfMessage)
                {
                    await OnMessageAsync(peer, Encoding.UTF8.GetString(message.ToArray()));
                    message.SetLength(0);
                }
            }
        }
        catch (Exception e) when (e is WebSocketException or OperationCanceledException)
        {
            _log.LogDebug("P2P link {Id} closed: {Message}", peer.Id, e.Message);
        }
        finally
        {
            Unregister(peer);
        }
    }

    // ------------------------------------------------------------------ handshake

    private async Task SendHelloAsync(PeerSession peer)
    {
        // Fresh ephemeral KEM key pair per connection: compromise of one session
        // never affects another (QKD-inspired key freshness).
        var kemPair = _keyExchange.GenerateKeyPair();
        peer.EphemeralKemPrivate = (Org.BouncyCastle.Crypto.Parameters.MLKemPrivateKeyParameters)kemPair.Private;
        string kemPub = _keyExchange.EncodePublicKey(
            (Org.BouncyCastle.Crypto.Parameters.MLKemPublicKeyParameters)kemPair.Public);

        await SendAsync(peer, new Dictionary<string, object?>
        {
            ["type"] = "HELLO",
            ["nodeId"] = _wallet.Address,
            ["dsaPub"] = _wallet.PublicKeyBase64,
            ["kemPub"] = kemPub,
            // Signature binds the ephemeral KEM key to this node's ML-DSA identity.
            ["signature"] = _wallet.Sign(kemPub + "|" + _wallet.Address)
        });
    }

    public async Task OnMessageAsync(PeerSession peer, string text)
    {
        try
        {
            var msg = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(text, JsonDefaults.Options)
                      ?? throw new ArgumentException("Empty P2P frame");
            switch (Str(msg, "type"))
            {
                case "HELLO":
                    await OnHelloAsync(peer, msg);
                    break;
                case "ENCAPS":
                    await OnEncapsAsync(peer, msg);
                    break;
                case "SECURE":
                    await OnSecureAsync(peer, msg);
                    break;
                default:
                    _log.LogWarning("Unknown P2P message type from {Id}", peer.Id);
                    break;
            }
        }
        catch (Exception e)
        {
            _log.LogWarning("Rejected P2P message ({Id}): {Message}", peer.Id, e.Message);
        }
    }

    private async Task OnHelloAsync(PeerSession peer, Dictionary<string, JsonElement> msg)
    {
        string nodeId = Str(msg, "nodeId");
        string dsaPub = Str(msg, "dsaPub");
        string kemPub = Str(msg, "kemPub");
        string signature = Str(msg, "signature");

        RequireAuthentic(nodeId, dsaPub, kemPub + "|" + nodeId, signature);

        // Derive the one-time session key and send the signed ciphertext back.
        PqcKeyExchangeService.Encapsulation enc = _keyExchange.Encapsulate(kemPub);
        peer.PeerNodeId = nodeId;
        peer.Establish(enc.SessionKey);

        await SendAsync(peer, new Dictionary<string, object?>
        {
            ["type"] = "ENCAPS",
            ["nodeId"] = _wallet.Address,
            ["dsaPub"] = _wallet.PublicKeyBase64,
            ["ciphertext"] = enc.CiphertextBase64,
            ["signature"] = _wallet.Sign(enc.CiphertextBase64 + "|" + _wallet.Address)
        });

        _events.Publish(new LedgerEvent(LedgerEvent.EventType.PEER_CONNECTED,
            new Dictionary<string, string> { ["nodeId"] = nodeId, ["direction"] = "inbound" }));
        _log.LogInformation("PQC channel established with inbound peer {NodeId}", nodeId);
    }

    private async Task OnEncapsAsync(PeerSession peer, Dictionary<string, JsonElement> msg)
    {
        string nodeId = Str(msg, "nodeId");
        string dsaPub = Str(msg, "dsaPub");
        string ciphertext = Str(msg, "ciphertext");
        string signature = Str(msg, "signature");

        RequireAuthentic(nodeId, dsaPub, ciphertext + "|" + nodeId, signature);

        var kemPrivate = peer.EphemeralKemPrivate
                         ?? throw new UnauthorizedAccessException("ENCAPS without a pending HELLO");
        peer.PeerNodeId = nodeId;
        peer.Establish(_keyExchange.Decapsulate(ciphertext, kemPrivate));

        _events.Publish(new LedgerEvent(LedgerEvent.EventType.PEER_CONNECTED,
            new Dictionary<string, string> { ["nodeId"] = nodeId, ["direction"] = "outbound" }));
        _log.LogInformation("PQC channel established with outbound peer {NodeId}", nodeId);

        // First act on a fresh secure channel: ask for the peer's ledger.
        await SendSecureAsync(peer, new Dictionary<string, object?> { ["type"] = "CHAIN_REQUEST" });
    }

    /// <summary>
    /// Verifies the handshake signature AND that the claimed node id is the fingerprint
    /// of the signing key — the identity binding that defeats key-substitution MITM.
    /// </summary>
    private void RequireAuthentic(string nodeId, string dsaPub, string signedContent, string signature)
    {
        if (!_signatures.Verify(signedContent, signature, dsaPub))
        {
            throw new UnauthorizedAccessException("Handshake rejected: invalid ML-DSA signature");
        }
        string fingerprint;
        try
        {
            fingerprint = _signatures.Fingerprint(_signatures.DecodePublicKey(dsaPub));
        }
        catch (ArgumentException)
        {
            throw new UnauthorizedAccessException("Handshake rejected: malformed ML-DSA key");
        }
        if (fingerprint != nodeId)
        {
            throw new UnauthorizedAccessException("Handshake rejected: node id is not the key fingerprint");
        }
    }

    // ------------------------------------------------------------------ secure traffic

    private async Task OnSecureAsync(PeerSession peer, Dictionary<string, JsonElement> msg)
    {
        if (!peer.Established)
        {
            throw new UnauthorizedAccessException("SECURE frame before handshake completion");
        }
        // GCM decryption throws on any tampering — integrity and confidentiality in one step.
        string plaintext = _channel.Decrypt(Str(msg, "payload"), peer.SessionKey!);
        var inner = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(plaintext, JsonDefaults.Options)
                    ?? throw new ArgumentException("Empty secure frame");

        switch (Str(inner, "type"))
        {
            case "CHAIN_REQUEST":
                await SendSecureAsync(peer, new Dictionary<string, object?>
                {
                    ["type"] = "CHAIN_RESPONSE",
                    ["blocks"] = _blockchain.Blocks()
                });
                break;
            case "CHAIN_RESPONSE":
                var remoteChain = inner["blocks"].Deserialize<List<Block>>(JsonDefaults.Options)
                                  ?? throw new ArgumentException("CHAIN_RESPONSE without blocks");
                bool adopted = _blockchain.ReplaceChain(remoteChain);
                _log.LogInformation("Chain from {NodeId}: length={Length} adopted={Adopted}",
                    peer.PeerNodeId, remoteChain.Count, adopted);
                break;
            case "ANNOUNCE":
                int remoteLength = inner["length"].GetInt32();
                if (remoteLength > _blockchain.Length)
                {
                    await SendSecureAsync(peer, new Dictionary<string, object?> { ["type"] = "CHAIN_REQUEST" });
                }
                break;
            default:
                _log.LogWarning("Unknown secure message from {NodeId}", peer.PeerNodeId);
                break;
        }
    }

    /// <summary>Asks every established peer for its chain; longest valid chain wins locally.</summary>
    public async Task<int> SyncWithPeersAsync()
    {
        int asked = 0;
        foreach (PeerSession peer in _peers.Values)
        {
            if (peer.Established)
            {
                await SendSecureAsync(peer, new Dictionary<string, object?> { ["type"] = "CHAIN_REQUEST" });
                asked++;
            }
        }
        return asked;
    }

    private void OnLedgerEvent(LedgerEvent ledgerEvent)
    {
        if (ledgerEvent.Type != LedgerEvent.EventType.BLOCK_ADDED)
        {
            return;
        }
        foreach (PeerSession peer in _peers.Values)
        {
            if (peer.Established)
            {
                // Fire-and-forget: gossip must never block the mining call that raised the event.
                _ = SendSecureAsync(peer, new Dictionary<string, object?>
                {
                    ["type"] = "ANNOUNCE",
                    ["length"] = _blockchain.Length
                });
            }
        }
    }

    private async Task SendSecureAsync(PeerSession peer, Dictionary<string, object?> inner)
    {
        try
        {
            string plaintext = JsonSerializer.Serialize(inner, JsonDefaults.Options);
            await SendAsync(peer, new Dictionary<string, object?>
            {
                ["type"] = "SECURE",
                ["payload"] = _channel.Encrypt(plaintext, peer.SessionKey!)
            });
        }
        catch (Exception e)
        {
            _log.LogWarning("Failed to send secure frame to {NodeId}: {Message}",
                peer.PeerNodeId, e.Message);
        }
    }

    private async Task SendAsync(PeerSession peer, Dictionary<string, object?> message)
    {
        byte[] json = JsonSerializer.SerializeToUtf8Bytes(message, JsonDefaults.Options);
        // One outstanding send per socket (WebSocket is not safe for concurrent writers).
        await peer.SendLock.WaitAsync();
        try
        {
            await peer.Socket.SendAsync(json, WebSocketMessageType.Text, endOfMessage: true,
                CancellationToken.None);
        }
        catch (Exception e)
        {
            _log.LogWarning("Failed to send P2P frame: {Message}", e.Message);
        }
        finally
        {
            peer.SendLock.Release();
        }
    }

    private static string Str(Dictionary<string, JsonElement> msg, string key)
        => msg.TryGetValue(key, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString()!
            : throw new ArgumentException("Missing P2P field: " + key);
}
