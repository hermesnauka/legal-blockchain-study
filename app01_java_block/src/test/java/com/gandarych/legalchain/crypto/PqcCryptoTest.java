package com.gandarych.legalchain.crypto;

import org.junit.jupiter.api.Test;

import javax.crypto.SecretKey;
import java.security.KeyPair;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/**
 * Verifies the post-quantum primitives end to end: ML-DSA signatures (FIPS 204),
 * ML-KEM key encapsulation (FIPS 203) and the AES-256-GCM secure channel built on it.
 */
class PqcCryptoTest {

    private final PqcSignatureService signatures = new PqcSignatureService();
    private final PqcKeyExchangeService keyExchange = new PqcKeyExchangeService();
    private final SecureChannel channel = new SecureChannel();

    @Test
    void signAndVerifyRoundTrip() {
        KeyPair keyPair = signatures.generateKeyPair();
        String content = "compliant ledger entry";
        String signature = signatures.sign(content, keyPair.getPrivate());

        assertThat(signatures.verify(content, signature,
                signatures.encodePublicKey(keyPair.getPublic()))).isTrue();
    }

    @Test
    void tamperedContentFailsVerification() {
        KeyPair keyPair = signatures.generateKeyPair();
        String signature = signatures.sign("original", keyPair.getPrivate());

        assertThat(signatures.verify("tampered", signature,
                signatures.encodePublicKey(keyPair.getPublic()))).isFalse();
    }

    @Test
    void signatureFromAnotherKeyIsRejected() {
        KeyPair alice = signatures.generateKeyPair();
        KeyPair mallory = signatures.generateKeyPair();
        String signature = signatures.sign("payment", mallory.getPrivate());

        assertThat(signatures.verify("payment", signature,
                signatures.encodePublicKey(alice.getPublic()))).isFalse();
    }

    @Test
    void kemEncapsulationYieldsSameSecretOnBothSides() {
        KeyPair receiver = keyExchange.generateKeyPair();

        PqcKeyExchangeService.Encapsulation sent =
                keyExchange.encapsulate(keyExchange.encodePublicKey(receiver.getPublic()));
        SecretKey received = keyExchange.decapsulate(sent.ciphertextBase64(), receiver.getPrivate());

        assertThat(received.getEncoded()).isEqualTo(sent.sessionKey().getEncoded());
        assertThat(received.getEncoded()).hasSize(32); // AES-256
    }

    @Test
    void secureChannelEncryptsAndAuthenticates() {
        KeyPair receiver = keyExchange.generateKeyPair();
        PqcKeyExchangeService.Encapsulation enc =
                keyExchange.encapsulate(keyExchange.encodePublicKey(receiver.getPublic()));

        String ciphertext = channel.encrypt("CHAIN_REQUEST", enc.sessionKey());
        assertThat(channel.decrypt(ciphertext, enc.sessionKey())).isEqualTo("CHAIN_REQUEST");

        // Any bit flip must fail the GCM authentication tag (MITM tamper detection).
        char flipped = ciphertext.charAt(10) == 'A' ? 'B' : 'A';
        String tampered = ciphertext.substring(0, 10) + flipped + ciphertext.substring(11);
        assertThatThrownBy(() -> channel.decrypt(tampered, enc.sessionKey()))
                .isInstanceOf(IllegalStateException.class);
    }
}
