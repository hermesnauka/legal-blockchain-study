using LegalChain.Crypto;
using Org.BouncyCastle.Crypto.Parameters;

namespace LegalChain.Tests;

/// <summary>
/// SEC-02 / SEC-04 / SEC-05 at the primitive level: ML-DSA-65 signing round-trips and
/// rejects corruption, ML-KEM-768 encapsulation derives the same 32-byte secret on both
/// sides, and the AES-256-GCM channel decrypts its own frames while loudly rejecting
/// tampered ones (the QKD-style disturbance detection).
/// </summary>
public class PqcCryptoTest
{
    private readonly PqcSignatureService _signatures = new();
    private readonly PqcKeyExchangeService _keyExchange = new();
    private readonly SecureChannel _channel = new();

    [Fact]
    public void MlDsaSignatureRoundTripsAndRejectsWrongKeyAndCorruption()
    {
        var keyPair = _signatures.GenerateKeyPair();
        var publicKey = (MLDsaPublicKeyParameters)keyPair.Public;
        var privateKey = (MLDsaPrivateKeyParameters)keyPair.Private;
        string publicKeyBase64 = _signatures.EncodePublicKey(publicKey);

        string content = "ledger-entry|42|lcabc|lcdef|10|TRANSFER|{}";
        string signature = _signatures.Sign(content, privateKey);

        Assert.True(_signatures.Verify(content, signature, publicKeyBase64));
        Assert.False(_signatures.Verify(content + "tampered", signature, publicKeyBase64),
            "signature must not verify altered content");

        var otherKey = (MLDsaPublicKeyParameters)_signatures.GenerateKeyPair().Public;
        Assert.False(_signatures.Verify(content, signature, _signatures.EncodePublicKey(otherKey)),
            "signature must not verify under a different key");

        byte[] corrupted = Convert.FromBase64String(signature);
        corrupted[10] ^= 0xFF;
        Assert.False(_signatures.Verify(content, Convert.ToBase64String(corrupted), publicKeyBase64),
            "corrupted signature must fail");
    }

    [Fact]
    public void FingerprintIsStablePseudonymousAddress()
    {
        var publicKey = (MLDsaPublicKeyParameters)_signatures.GenerateKeyPair().Public;
        string fingerprint = _signatures.Fingerprint(publicKey);
        Assert.StartsWith("lc", fingerprint);
        Assert.Equal(42, fingerprint.Length);
        Assert.Equal(fingerprint, _signatures.Fingerprint(publicKey));
    }

    [Fact]
    public void MlKemEncapsulationDerivesTheSameSessionKeyOnBothSides()
    {
        var receiverPair = _keyExchange.GenerateKeyPair();
        string receiverPublic = _keyExchange.EncodePublicKey(
            (MLKemPublicKeyParameters)receiverPair.Public);

        PqcKeyExchangeService.Encapsulation enc = _keyExchange.Encapsulate(receiverPublic);
        byte[] receiverSecret = _keyExchange.Decapsulate(
            enc.CiphertextBase64, (MLKemPrivateKeyParameters)receiverPair.Private);

        Assert.Equal(32, enc.SessionKey.Length); // exactly an AES-256 key
        Assert.Equal(enc.SessionKey, receiverSecret);

        // QKD-inspired freshness: every encapsulation yields a new one-time key.
        Assert.NotEqual(enc.SessionKey, _keyExchange.Encapsulate(receiverPublic).SessionKey);
    }

    [Fact]
    public void SecureChannelEncryptsAndLoudlyRejectsTampering()
    {
        var receiverPair = _keyExchange.GenerateKeyPair();
        string receiverPublic = _keyExchange.EncodePublicKey(
            (MLKemPublicKeyParameters)receiverPair.Public);
        byte[] sessionKey = _keyExchange.Encapsulate(receiverPublic).SessionKey;

        string plaintext = "{\"type\":\"CHAIN_REQUEST\"}";
        string frame = _channel.Encrypt(plaintext, sessionKey);
        Assert.Equal(plaintext, _channel.Decrypt(frame, sessionKey));

        byte[] tampered = Convert.FromBase64String(frame);
        tampered[^1] ^= 0x01;
        Assert.Throws<InvalidOperationException>(
            () => _channel.Decrypt(Convert.ToBase64String(tampered), sessionKey));

        byte[] wrongKey = _keyExchange.Encapsulate(receiverPublic).SessionKey;
        Assert.Throws<InvalidOperationException>(() => _channel.Decrypt(frame, wrongKey));
    }
}
