using System.Text;
using Org.BouncyCastle.Crypto;
using Org.BouncyCastle.Crypto.Generators;
using Org.BouncyCastle.Crypto.Parameters;
using Org.BouncyCastle.Crypto.Signers;
using Org.BouncyCastle.Security;

namespace LegalChain.Crypto;

/// <summary>
/// Post-quantum digital signatures using <b>ML-DSA-65</b> (NIST FIPS 204, standardized
/// from CRYSTALS-Dilithium), provided by the BouncyCastle.Cryptography library.
/// </summary>
/// <remarks>
/// <para><b>Why this is compliant and secure:</b> classical blockchain signatures
/// (Bitcoin/Ethereum ECDSA over secp256k1) rely on the discrete-logarithm problem,
/// which Shor's algorithm solves in polynomial time on a large fault-tolerant quantum
/// computer. ML-DSA instead rests on the hardness of lattice problems (Module-LWE /
/// Module-SIS), for which no quantum speed-up is known. NIST finalized it as FIPS 204
/// in August 2024; using a NIST-standardized parameter set (ML-DSA-65, NIST security
/// category 3, ~AES-192 equivalent) is what makes the ledger auditable against a
/// concrete regulatory benchmark rather than ad-hoc cryptography.</para>
/// <para>Every ledger transaction carries the sender's ML-DSA public key and signature, so
/// any node — or any auditor — can independently verify authorship without contacting
/// the author. Identity stays pseudonymous: the "address" is only a SHA3-256 fingerprint
/// of the public key (self-sovereign identity), never a name.</para>
/// </remarks>
public class PqcSignatureService
{
    private static readonly MLDsaParameters Parameters = MLDsaParameters.ml_dsa_65;
    private static readonly SecureRandom Random = new();

    /// <summary>Human-readable algorithm label exposed over the API and in the education module.</summary>
    public string AlgorithmName => "ML-DSA-65 (FIPS 204 / CRYSTALS-Dilithium)";

    public AsymmetricCipherKeyPair GenerateKeyPair()
    {
        var generator = new MLDsaKeyPairGenerator();
        generator.Init(new MLDsaKeyGenerationParameters(Random, Parameters));
        return generator.GenerateKeyPair();
    }

    /// <summary>Signs UTF-8 content and returns the signature as Base64.</summary>
    public string Sign(string content, MLDsaPrivateKeyParameters privateKey)
    {
        var signer = new MLDsaSigner(Parameters, deterministic: false);
        signer.Init(forSigning: true, privateKey);
        byte[] bytes = Encoding.UTF8.GetBytes(content);
        signer.BlockUpdate(bytes, 0, bytes.Length);
        return Convert.ToBase64String(signer.GenerateSignature());
    }

    /// <summary>Verifies a Base64 signature against UTF-8 content and a Base64-encoded public key.</summary>
    public bool Verify(string content, string? signatureBase64, string? publicKeyBase64)
    {
        if (signatureBase64 is null || publicKeyBase64 is null)
        {
            return false;
        }
        try
        {
            MLDsaPublicKeyParameters publicKey = DecodePublicKey(publicKeyBase64);
            var signer = new MLDsaSigner(Parameters, deterministic: false);
            signer.Init(forSigning: false, publicKey);
            byte[] bytes = Encoding.UTF8.GetBytes(content);
            signer.BlockUpdate(bytes, 0, bytes.Length);
            return signer.VerifySignature(Convert.FromBase64String(signatureBase64));
        }
        catch (Exception)
        {
            // A malformed key or signature is simply an invalid signature, not a server error.
            return false;
        }
    }

    /// <summary>Decodes a Base64 ML-DSA public key; throws <see cref="ArgumentException"/> if malformed.</summary>
    public MLDsaPublicKeyParameters DecodePublicKey(string publicKeyBase64)
    {
        try
        {
            return MLDsaPublicKeyParameters.FromEncoding(Parameters, Convert.FromBase64String(publicKeyBase64));
        }
        catch (Exception e)
        {
            throw new ArgumentException("Malformed ML-DSA public key", e);
        }
    }

    public string EncodePublicKey(MLDsaPublicKeyParameters publicKey)
        => Convert.ToBase64String(publicKey.GetEncoded());

    /// <summary>
    /// Pseudonymous address: SHA3-256 fingerprint of the encoded public key. This is the
    /// ZKP-flavoured privacy property of the ledger — parties prove control of a key
    /// (by producing valid ML-DSA signatures) without ever revealing a real-world identity.
    /// </summary>
    public string Fingerprint(MLDsaPublicKeyParameters publicKey)
        => "lc" + HashUtil.Sha3(EncodePublicKey(publicKey))[..40];
}
