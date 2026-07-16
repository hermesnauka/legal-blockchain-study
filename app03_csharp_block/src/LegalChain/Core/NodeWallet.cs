using LegalChain.Crypto;
using Org.BouncyCastle.Crypto.Parameters;

namespace LegalChain.Core;

/// <summary>
/// The node's own post-quantum wallet: an ML-DSA-65 key pair generated at startup.
/// </summary>
/// <remarks>
/// Self-sovereign identity in practice: the node's address is the SHA3-256
/// fingerprint of its public key. No registration authority issues it, no third party
/// can revoke it, and it reveals nothing about the operator — yet every signature made
/// with it is independently verifiable by anyone. The private key never leaves this
/// process (in the MVP it is intentionally not persisted: restart = new identity).
/// </remarks>
public class NodeWallet
{
    private readonly PqcSignatureService _signatures;
    private readonly MLDsaPrivateKeyParameters _privateKey;
    private readonly MLDsaPublicKeyParameters _publicKey;

    public NodeWallet(PqcSignatureService signatures)
    {
        _signatures = signatures;
        var keyPair = signatures.GenerateKeyPair();
        _privateKey = (MLDsaPrivateKeyParameters)keyPair.Private;
        _publicKey = (MLDsaPublicKeyParameters)keyPair.Public;
        Address = signatures.Fingerprint(_publicKey);
    }

    public string Address { get; }

    public string PublicKeyBase64 => _signatures.EncodePublicKey(_publicKey);

    public string Sign(string content) => _signatures.Sign(content, _privateKey);

    public string Algorithm => _signatures.AlgorithmName;
}
