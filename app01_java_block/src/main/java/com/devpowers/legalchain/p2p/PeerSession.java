package com.devpowers.legalchain.p2p;

import org.springframework.web.socket.WebSocketSession;

import javax.crypto.SecretKey;
import java.security.PrivateKey;

/**
 * Mutable handshake state for one peer link. A link becomes {@code established} only
 * after the ML-DSA-authenticated ML-KEM handshake has produced a one-time AES-256
 * session key; until then no ledger data flows.
 */
public class PeerSession {

    private final WebSocketSession ws;
    private final boolean initiator;
    private volatile String peerNodeId;
    private volatile String url;
    /** Initiator's ephemeral ML-KEM private key, discarded once the session key is derived. */
    private volatile PrivateKey ephemeralKemPrivate;
    private volatile SecretKey sessionKey;

    public PeerSession(WebSocketSession ws, boolean initiator) {
        this.ws = ws;
        this.initiator = initiator;
    }

    public WebSocketSession ws() {
        return ws;
    }

    public boolean initiator() {
        return initiator;
    }

    public String peerNodeId() {
        return peerNodeId;
    }

    public void peerNodeId(String peerNodeId) {
        this.peerNodeId = peerNodeId;
    }

    public String url() {
        return url;
    }

    public void url(String url) {
        this.url = url;
    }

    public PrivateKey ephemeralKemPrivate() {
        return ephemeralKemPrivate;
    }

    public void ephemeralKemPrivate(PrivateKey key) {
        this.ephemeralKemPrivate = key;
    }

    public SecretKey sessionKey() {
        return sessionKey;
    }

    public void establish(SecretKey key) {
        this.sessionKey = key;
        this.ephemeralKemPrivate = null; // forward secrecy: ephemeral key is single-use
    }

    public boolean established() {
        return sessionKey != null;
    }
}
