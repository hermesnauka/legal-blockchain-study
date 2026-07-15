package com.devpowers.legalchain.crypto;

import org.springframework.stereotype.Service;

import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.SecureRandom;
import java.util.Base64;

/**
 * Authenticated symmetric channel: AES-256-GCM under the ML-KEM-derived session key.
 *
 * <p>GCM provides confidentiality <i>and</i> integrity in one primitive: any bit flipped
 * in transit fails the authentication tag and the message is rejected, so a
 * man-in-the-middle who cannot break the KEM cannot silently alter ledger data either.
 * A fresh random 96-bit IV is generated per message and prepended to the ciphertext
 * (an IV must never repeat under the same GCM key; since our session keys are one-time
 * ML-KEM secrets, random IVs are safe at these message volumes).</p>
 */
@Service
public class SecureChannel {

    private static final int IV_BYTES = 12;
    private static final int TAG_BITS = 128;
    private final SecureRandom random = new SecureRandom();

    /** Encrypts UTF-8 plaintext; returns Base64(iv || ciphertext+tag). */
    public String encrypt(String plaintext, SecretKey sessionKey) {
        try {
            byte[] iv = new byte[IV_BYTES];
            random.nextBytes(iv);
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, sessionKey, new GCMParameterSpec(TAG_BITS, iv));
            byte[] ciphertext = cipher.doFinal(plaintext.getBytes(StandardCharsets.UTF_8));
            return Base64.getEncoder().encodeToString(
                    ByteBuffer.allocate(iv.length + ciphertext.length).put(iv).put(ciphertext).array());
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("AES-GCM encryption failed", e);
        }
    }

    /** Decrypts Base64(iv || ciphertext+tag); throws if the authentication tag is invalid. */
    public String decrypt(String payloadBase64, SecretKey sessionKey) {
        try {
            byte[] payload = Base64.getDecoder().decode(payloadBase64);
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, sessionKey,
                    new GCMParameterSpec(TAG_BITS, payload, 0, IV_BYTES));
            byte[] plaintext = cipher.doFinal(payload, IV_BYTES, payload.length - IV_BYTES);
            return new String(plaintext, StandardCharsets.UTF_8);
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("AES-GCM decryption failed (tampered or wrong key)", e);
        }
    }
}
