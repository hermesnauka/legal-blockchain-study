using Org.BouncyCastle.Crypto;
using Org.BouncyCastle.Crypto.Generators;
using Org.BouncyCastle.Crypto.Kems;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.Security;

namespace LegalChain.Crypto;

/// <summary>
/// Post-quantum key establishment using <b>ML-KEM-768</b> (NIST FIPS 203, standardized
/// from CRYSTALS-Kyber), provided by the BouncyCastle.Cryptography library.
/// </summary>
/// <remarks>
/// <para><b>QKD-inspired design.</b> True Quantum Key Distribution needs dedicated optical
/// hardware, but its two security ideas can be reproduced in software: (1) the shared
/// symmetric key is <i>freshly established per session</i> and never stored or reused, and
/// (2) its secrecy does not depend on any key that a "harvest now, decrypt later"
/// adversary could break retroactively — ML-KEM's security rests on the Module-LWE
/// lattice problem, against which Shor's algorithm gives no advantage. Each P2P session
/// therefore gets a one-time AES-256 key that a quantum-equipped eavesdropper recording
/// today's traffic still cannot recover.</para>
/// <para><b>MITM resistance</b> comes from binding this KEM exchange to ML-DSA identities:
/// the handshake messages that carry the KEM public key and the ciphertext are signed
/// with the sender's ML-DSA key (see <c>P2p.P2pService</c>), so an attacker cannot
/// substitute their own KEM key without forging a post-quantum signature.</para>
/// </remarks>
public class PqcKeyExchangeService
{
    private static readonly MLKemParameters Parameters = MLKemParameters.ml_kem_768;
    private static readonly SecureRandom Random = new();

    public string AlgorithmName => "ML-KEM-768 (FIPS 203 / CRYSTALS-Kyber)";

    public AsymmetricCipherKeyPair GenerateKeyPair()
    {
        var generator = new MLKemKeyPairGenerator();
        generator.Init(new MLKemKeyGenerationParameters(Random, Parameters));
        return generator.GenerateKeyPair();
    }

    /// <summary>
    /// Sender side: encapsulates a fresh shared secret against the receiver's public key.
    /// Returns the ciphertext to transmit plus the derived AES-256 session key.
    /// </summary>
    public Encapsulation Encapsulate(string receiverPublicKeyBase64)
    {
        MLKemPublicKeyParameters receiverKey = DecodePublicKey(receiverPublicKeyBase64);
        var encapsulator = new MLKemEncapsulator(Parameters);
        encapsulator.Init(new ParametersWithRandom(receiverKey, Random));
        byte[] ciphertext = new byte[encapsulator.EncapsulationLength];
        // ML-KEM-768 yields a 32-byte shared secret — exactly an AES-256 key.
        byte[] sessionKey = new byte[encapsulator.SecretLength];
        encapsulator.Encapsulate(ciphertext, 0, ciphertext.Length, sessionKey, 0, sessionKey.Length);
        return new Encapsulation(Convert.ToBase64String(ciphertext), sessionKey);
    }

    /// <summary>Receiver side: recovers the same AES-256 session key from the transmitted ciphertext.</summary>
    public byte[] Decapsulate(string ciphertextBase64, MLKemPrivateKeyParameters privateKey)
    {
        var decapsulator = new MLKemDecapsulator(Parameters);
        decapsulator.Init(privateKey);
        byte[] ciphertext = Convert.FromBase64String(ciphertextBase64);
        byte[] sessionKey = new byte[decapsulator.SecretLength];
        decapsulator.Decapsulate(ciphertext, 0, ciphertext.Length, sessionKey, 0, sessionKey.Length);
        return sessionKey;
    }

    public string EncodePublicKey(MLKemPublicKeyParameters publicKey)
        => Convert.ToBase64String(publicKey.GetEncoded());

    private static MLKemPublicKeyParameters DecodePublicKey(string publicKeyBase64)
    {
        try
        {
            return MLKemPublicKeyParameters.FromEncoding(Parameters, Convert.FromBase64String(publicKeyBase64));
        }
        catch (Exception e)
        {
            throw new ArgumentException("Malformed ML-KEM public key", e);
        }
    }

    /// <summary>Result of KEM encapsulation: what travels on the wire + the local session key.</summary>
    public record Encapsulation(string CiphertextBase64, byte[] SessionKey);
}
