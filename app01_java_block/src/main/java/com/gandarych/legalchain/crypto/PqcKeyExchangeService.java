package com.gandarych.legalchain.crypto;

import org.bouncycastle.jcajce.spec.MLKEMParameterSpec;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.springframework.stereotype.Service;

import javax.crypto.KEM;
import javax.crypto.SecretKey;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Security;
import java.security.spec.X509EncodedKeySpec;
import java.util.Base64;

/**
 * Post-quantum key establishment using <b>ML-KEM-768</b> (NIST FIPS 203, standardized
 * from CRYSTALS-Kyber) through the Java 21 {@link javax.crypto.KEM} API (JEP 452) and
 * the BouncyCastle provider.
 *
 * <p><b>QKD-inspired design.</b> True Quantum Key Distribution needs dedicated optical
 * hardware, but its two security ideas can be reproduced in software: (1) the shared
 * symmetric key is <i>freshly established per session</i> and never stored or reused, and
 * (2) its secrecy does not depend on any key that a "harvest now, decrypt later"
 * adversary could break retroactively — ML-KEM's security rests on the Module-LWE
 * lattice problem, against which Shor's algorithm gives no advantage. Each P2P session
 * therefore gets a one-time AES-256 key that a quantum-equipped eavesdropper recording
 * today's traffic still cannot recover.</p>
 *
 * <p><b>MITM resistance</b> comes from binding this KEM exchange to ML-DSA identities:
 * the handshake messages that carry the KEM public key and the ciphertext are signed
 * with the sender's ML-DSA key (see {@code p2p.P2pProtocolService}), so an attacker
 * cannot substitute their own KEM key without forging a post-quantum signature.</p>
 */
@Service
public class PqcKeyExchangeService {

    static {
        if (Security.getProvider(BouncyCastleProvider.PROVIDER_NAME) == null) {
            Security.addProvider(new BouncyCastleProvider());
        }
    }

    private static final String ALGORITHM = "ML-KEM";
    /** ML-KEM produces a 32-byte shared secret — exactly an AES-256 key. */
    private static final int SHARED_SECRET_BYTES = 32;

    public String algorithmName() {
        return "ML-KEM-768 (FIPS 203 / CRYSTALS-Kyber)";
    }

    public KeyPair generateKeyPair() {
        try {
            KeyPairGenerator generator =
                    KeyPairGenerator.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
            generator.initialize(MLKEMParameterSpec.ml_kem_768);
            return generator.generateKeyPair();
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("Cannot generate ML-KEM key pair", e);
        }
    }

    /**
     * Sender side: encapsulates a fresh shared secret against the receiver's public key.
     * Returns the ciphertext to transmit plus the derived AES-256 session key.
     */
    public Encapsulation encapsulate(String receiverPublicKeyBase64) {
        try {
            PublicKey receiverKey = decodePublicKey(receiverPublicKeyBase64);
            KEM kem = KEM.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
            KEM.Encapsulated encapsulated = kem.newEncapsulator(receiverKey)
                    .encapsulate(0, SHARED_SECRET_BYTES, "AES");
            return new Encapsulation(
                    Base64.getEncoder().encodeToString(encapsulated.encapsulation()),
                    encapsulated.key());
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("ML-KEM encapsulation failed", e);
        }
    }

    /** Receiver side: recovers the same AES-256 session key from the transmitted ciphertext. */
    public SecretKey decapsulate(String ciphertextBase64, PrivateKey privateKey) {
        try {
            KEM kem = KEM.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
            return kem.newDecapsulator(privateKey)
                    .decapsulate(Base64.getDecoder().decode(ciphertextBase64), 0, SHARED_SECRET_BYTES, "AES");
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("ML-KEM decapsulation failed", e);
        }
    }

    public String encodePublicKey(PublicKey publicKey) {
        return Base64.getEncoder().encodeToString(publicKey.getEncoded());
    }

    private PublicKey decodePublicKey(String publicKeyBase64) throws GeneralSecurityException {
        KeyFactory factory = KeyFactory.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
        return factory.generatePublic(new X509EncodedKeySpec(Base64.getDecoder().decode(publicKeyBase64)));
    }

    /** Result of KEM encapsulation: what travels on the wire + the local session key. */
    public record Encapsulation(String ciphertextBase64, SecretKey sessionKey) {
    }
}
