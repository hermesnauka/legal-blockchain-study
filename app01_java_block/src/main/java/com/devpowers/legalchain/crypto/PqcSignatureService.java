package com.devpowers.legalchain.crypto;

import org.bouncycastle.jcajce.spec.MLDSAParameterSpec;
import org.bouncycastle.jce.provider.BouncyCastleProvider;
import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Security;
import java.security.Signature;
import java.security.spec.X509EncodedKeySpec;
import java.util.Base64;

/**
 * Post-quantum digital signatures using <b>ML-DSA-65</b> (NIST FIPS 204, standardized
 * from CRYSTALS-Dilithium), provided by BouncyCastle.
 *
 * <p><b>Why this is compliant and secure:</b> classical blockchain signatures
 * (Bitcoin/Ethereum ECDSA over secp256k1) rely on the discrete-logarithm problem,
 * which Shor's algorithm solves in polynomial time on a large fault-tolerant quantum
 * computer. ML-DSA instead rests on the hardness of lattice problems (Module-LWE /
 * Module-SIS), for which no quantum speed-up is known. NIST finalized it as FIPS 204
 * in August 2024; using a NIST-standardized parameter set (ML-DSA-65, NIST security
 * category 3, ~AES-192 equivalent) is what makes the ledger auditable against a
 * concrete regulatory benchmark rather than ad-hoc cryptography.</p>
 *
 * <p>Every ledger transaction carries the sender's ML-DSA public key and signature, so
 * any node — or any auditor — can independently verify authorship without contacting
 * the author. Identity stays pseudonymous: the "address" is only a SHA3-256 fingerprint
 * of the public key (self-sovereign identity), never a name.</p>
 */
@Service
public class PqcSignatureService {

    static {
        if (Security.getProvider(BouncyCastleProvider.PROVIDER_NAME) == null) {
            Security.addProvider(new BouncyCastleProvider());
        }
    }

    private static final String ALGORITHM = "ML-DSA";

    /** Human-readable algorithm label exposed over the API and in the education module. */
    public String algorithmName() {
        return "ML-DSA-65 (FIPS 204 / CRYSTALS-Dilithium)";
    }

    public KeyPair generateKeyPair() {
        try {
            KeyPairGenerator generator =
                    KeyPairGenerator.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
            generator.initialize(MLDSAParameterSpec.ml_dsa_65);
            return generator.generateKeyPair();
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("Cannot generate ML-DSA key pair", e);
        }
    }

    /** Signs UTF-8 content and returns the signature as Base64. */
    public String sign(String content, PrivateKey privateKey) {
        try {
            Signature signature = Signature.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
            signature.initSign(privateKey);
            signature.update(content.getBytes(StandardCharsets.UTF_8));
            return Base64.getEncoder().encodeToString(signature.sign());
        } catch (GeneralSecurityException e) {
            throw new IllegalStateException("ML-DSA signing failed", e);
        }
    }

    /** Verifies a Base64 signature against UTF-8 content and a Base64 X.509 public key. */
    public boolean verify(String content, String signatureBase64, String publicKeyBase64) {
        try {
            PublicKey publicKey = decodePublicKey(publicKeyBase64);
            Signature signature = Signature.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
            signature.initVerify(publicKey);
            signature.update(content.getBytes(StandardCharsets.UTF_8));
            return signature.verify(Base64.getDecoder().decode(signatureBase64));
        } catch (GeneralSecurityException | IllegalArgumentException e) {
            // A malformed key or signature is simply an invalid signature, not a server error.
            return false;
        }
    }

    public PublicKey decodePublicKey(String publicKeyBase64) throws GeneralSecurityException {
        KeyFactory factory = KeyFactory.getInstance(ALGORITHM, BouncyCastleProvider.PROVIDER_NAME);
        return factory.generatePublic(new X509EncodedKeySpec(Base64.getDecoder().decode(publicKeyBase64)));
    }

    public String encodePublicKey(PublicKey publicKey) {
        return Base64.getEncoder().encodeToString(publicKey.getEncoded());
    }

    /**
     * Pseudonymous address: SHA3-256 fingerprint of the encoded public key. This is the
     * ZKP-flavoured privacy property of the ledger — parties prove control of a key
     * (by producing valid ML-DSA signatures) without ever revealing a real-world identity.
     */
    public String fingerprint(PublicKey publicKey) {
        return "lc" + HashUtil.sha3(encodePublicKey(publicKey)).substring(0, 40);
    }
}
