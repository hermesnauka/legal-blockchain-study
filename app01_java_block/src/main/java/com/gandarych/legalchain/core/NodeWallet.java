package com.gandarych.legalchain.core;

import com.gandarych.legalchain.crypto.PqcSignatureService;
import org.springframework.stereotype.Component;

import java.security.KeyPair;

/**
 * The node's own post-quantum wallet: an ML-DSA-65 key pair generated at startup.
 *
 * <p>Self-sovereign identity in practice: the node's address is the SHA3-256
 * fingerprint of its public key. No registration authority issues it, no third party
 * can revoke it, and it reveals nothing about the operator — yet every signature made
 * with it is independently verifiable by anyone. The private key never leaves this
 * process (in the MVP it is intentionally not persisted: restart = new identity).</p>
 */
@Component
public class NodeWallet {

    private final PqcSignatureService signatures;
    private final KeyPair keyPair;
    private final String address;

    public NodeWallet(PqcSignatureService signatures) {
        this.signatures = signatures;
        this.keyPair = signatures.generateKeyPair();
        this.address = signatures.fingerprint(keyPair.getPublic());
    }

    public String address() {
        return address;
    }

    public String publicKeyBase64() {
        return signatures.encodePublicKey(keyPair.getPublic());
    }

    public String sign(String content) {
        return signatures.sign(content, keyPair.getPrivate());
    }

    public String algorithm() {
        return signatures.algorithmName();
    }
}
