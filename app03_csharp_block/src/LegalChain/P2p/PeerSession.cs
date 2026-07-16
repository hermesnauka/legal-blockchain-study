using System.Net.WebSockets;
using Org.BouncyCastle.Crypto.Parameters;

namespace LegalChain.P2p;

/// <summary>
/// Mutable handshake state for one peer link. A link becomes <see cref="Established"/>
/// only after the ML-DSA-authenticated ML-KEM handshake has produced a one-time AES-256
/// session key; until then no ledger data flows.
/// </summary>
public class PeerSession
{
    public PeerSession(WebSocket socket, bool initiator)
    {
        Socket = socket;
        Initiator = initiator;
    }

    public string Id { get; } = Guid.NewGuid().ToString();

    public WebSocket Socket { get; }

    public bool Initiator { get; }

    public string? PeerNodeId { get; set; }

    public string? Url { get; set; }

    /// <summary>Initiator's ephemeral ML-KEM private key, discarded once the session key is derived.</summary>
    public MLKemPrivateKeyParameters? EphemeralKemPrivate { get; set; }

    public byte[]? SessionKey { get; private set; }

    /// <summary>Serializes concurrent sends: a WebSocket allows only one outstanding send.</summary>
    public SemaphoreSlim SendLock { get; } = new(1, 1);

    public void Establish(byte[] sessionKey)
    {
        SessionKey = sessionKey;
        EphemeralKemPrivate = null; // forward secrecy: ephemeral key is single-use
    }

    public bool Established => SessionKey is not null;
}
