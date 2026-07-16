using System.Security.Cryptography;
using System.Text;

namespace LegalChain.Crypto;

/// <summary>
/// Authenticated symmetric channel: AES-256-GCM under the ML-KEM-derived session key.
/// </summary>
/// <remarks>
/// GCM provides confidentiality <i>and</i> integrity in one primitive: any bit flipped
/// in transit fails the authentication tag and the message is rejected, so a
/// man-in-the-middle who cannot break the KEM cannot silently alter ledger data either.
/// A fresh random 96-bit nonce is generated per message and prepended to the ciphertext
/// (a nonce must never repeat under the same GCM key; since our session keys are one-time
/// ML-KEM secrets, random nonces are safe at these message volumes).
/// </remarks>
public class SecureChannel
{
    private const int NonceBytes = 12;
    private const int TagBytes = 16;

    /// <summary>Encrypts UTF-8 plaintext; returns Base64(nonce || ciphertext || tag).</summary>
    public string Encrypt(string plaintext, byte[] sessionKey)
    {
        byte[] nonce = RandomNumberGenerator.GetBytes(NonceBytes);
        byte[] plainBytes = Encoding.UTF8.GetBytes(plaintext);
        byte[] ciphertext = new byte[plainBytes.Length];
        byte[] tag = new byte[TagBytes];
        using var gcm = new AesGcm(sessionKey, TagBytes);
        gcm.Encrypt(nonce, plainBytes, ciphertext, tag);
        byte[] payload = new byte[NonceBytes + ciphertext.Length + TagBytes];
        nonce.CopyTo(payload, 0);
        ciphertext.CopyTo(payload, NonceBytes);
        tag.CopyTo(payload, NonceBytes + ciphertext.Length);
        return Convert.ToBase64String(payload);
    }

    /// <summary>Decrypts Base64(nonce || ciphertext || tag); throws if the authentication tag is invalid.</summary>
    public string Decrypt(string payloadBase64, byte[] sessionKey)
    {
        byte[] payload = Convert.FromBase64String(payloadBase64);
        if (payload.Length < NonceBytes + TagBytes)
        {
            throw new InvalidOperationException("AES-GCM payload too short (tampered)");
        }
        var nonce = payload.AsSpan(0, NonceBytes);
        var ciphertext = payload.AsSpan(NonceBytes, payload.Length - NonceBytes - TagBytes);
        var tag = payload.AsSpan(payload.Length - TagBytes, TagBytes);
        byte[] plaintext = new byte[ciphertext.Length];
        using var gcm = new AesGcm(sessionKey, TagBytes);
        try
        {
            gcm.Decrypt(nonce, ciphertext, tag, plaintext);
        }
        catch (AuthenticationTagMismatchException e)
        {
            // The QKD-inspired "disturbance detection": tampering is loudly visible.
            throw new InvalidOperationException("AES-GCM decryption failed (tampered or wrong key)", e);
        }
        return Encoding.UTF8.GetString(plaintext);
    }
}
