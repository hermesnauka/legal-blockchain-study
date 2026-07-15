package com.devpowers.legalchain.p2p;

import com.devpowers.legalchain.core.Block;
import com.devpowers.legalchain.core.Blockchain;
import com.devpowers.legalchain.core.LedgerEvent;
import com.devpowers.legalchain.core.NodeWallet;
import com.devpowers.legalchain.crypto.PqcKeyExchangeService;
import com.devpowers.legalchain.crypto.PqcSignatureService;
import com.devpowers.legalchain.crypto.SecureChannel;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.ApplicationEventPublisher;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Service;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketSession;

import java.io.IOException;
import java.security.KeyPair;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * The P2P ledger-synchronization protocol between two remote nodes, secured end-to-end
 * with post-quantum cryptography.
 *
 * <h2>Handshake (QKD-inspired, MITM-resistant)</h2>
 * <ol>
 *   <li><b>HELLO</b> (initiator → responder): the initiator's node id, its ML-DSA public
 *       key, a <i>fresh ephemeral</i> ML-KEM public key, and an ML-DSA signature over
 *       {@code kemPublicKey|nodeId}.</li>
 *   <li><b>ENCAPS</b> (responder → initiator): the responder encapsulates a random
 *       32-byte secret against the initiator's KEM key and returns the ciphertext,
 *       signed with its own ML-DSA key over {@code ciphertext|nodeId}.</li>
 *   <li>Both sides now hold the same one-time AES-256-GCM session key; every subsequent
 *       frame is a <b>SECURE</b> message (authenticated encryption).</li>
 * </ol>
 *
 * <h2>Why a man-in-the-middle fails</h2>
 * The classic MITM move is substituting key material in flight. Here both handshake
 * messages are signed with ML-DSA, and each side checks that the claimed node id equals
 * the SHA3-256 fingerprint of the signing key. An attacker who swaps the KEM key or the
 * ciphertext must forge an ML-DSA signature — believed infeasible even for a quantum
 * adversary (lattice hardness, NIST FIPS 204). Tampering with SECURE frames fails the
 * GCM authentication tag instead.
 *
 * <h2>Trust without revealing identity (ZKP-style pseudonymity)</h2>
 * Note what the handshake does <i>not</i> contain: names, certificates, IP-bound
 * identities. Each party proves exactly one statement — "I control the private key
 * behind this fingerprint" — by producing a valid signature, and nothing else. This is
 * the zero-knowledge-flavoured privacy model of self-sovereign identity: trust is
 * established between pseudonyms, and even the adopted ledger data is never trusted on
 * channel security alone — {@link Blockchain#replaceChain} re-audits every block, every
 * signature and every consensus proof locally before accepting a peer's chain.
 */
@Service
public class P2pService {

    private static final Logger log = LoggerFactory.getLogger(P2pService.class);

    private final Blockchain blockchain;
    private final NodeWallet wallet;
    private final PqcSignatureService signatures;
    private final PqcKeyExchangeService keyExchange;
    private final SecureChannel channel;
    private final ObjectMapper mapper;
    private final ApplicationEventPublisher events;

    /** Live peer links keyed by WebSocket session id. */
    private final Map<String, PeerSession> peers = new ConcurrentHashMap<>();

    public P2pService(Blockchain blockchain, NodeWallet wallet, PqcSignatureService signatures,
                      PqcKeyExchangeService keyExchange, SecureChannel channel,
                      ObjectMapper mapper, ApplicationEventPublisher events) {
        this.blockchain = blockchain;
        this.wallet = wallet;
        this.signatures = signatures;
        this.keyExchange = keyExchange;
        this.channel = channel;
        this.mapper = mapper;
        this.events = events;
    }

    // ------------------------------------------------------------------ lifecycle

    public void register(WebSocketSession ws, boolean initiator, String url) {
        PeerSession peer = new PeerSession(ws, initiator);
        peer.url(url);
        peers.put(ws.getId(), peer);
        if (initiator) {
            sendHello(peer);
        }
    }

    public void unregister(WebSocketSession ws) {
        peers.remove(ws.getId());
    }

    public List<Map<String, String>> connectedPeers() {
        return peers.values().stream()
                .filter(PeerSession::established)
                .map(p -> Map.of(
                        "nodeId", p.peerNodeId() == null ? "?" : p.peerNodeId(),
                        "url", p.url() == null ? "(inbound)" : p.url()))
                .toList();
    }

    // ------------------------------------------------------------------ handshake

    private void sendHello(PeerSession peer) {
        // Fresh ephemeral KEM key pair per connection: compromise of one session
        // never affects another (QKD-inspired key freshness).
        KeyPair kemPair = keyExchange.generateKeyPair();
        peer.ephemeralKemPrivate(kemPair.getPrivate());
        String kemPub = keyExchange.encodePublicKey(kemPair.getPublic());

        send(peer, Map.of(
                "type", "HELLO",
                "nodeId", wallet.address(),
                "dsaPub", wallet.publicKeyBase64(),
                "kemPub", kemPub,
                // Signature binds the ephemeral KEM key to this node's ML-DSA identity.
                "signature", wallet.sign(kemPub + "|" + wallet.address())));
    }

    public void onMessage(WebSocketSession ws, String text) {
        PeerSession peer = peers.get(ws.getId());
        if (peer == null) {
            return;
        }
        try {
            Map<String, Object> msg = mapper.readValue(text, new TypeReference<>() {
            });
            switch (String.valueOf(msg.get("type"))) {
                case "HELLO" -> onHello(peer, msg);
                case "ENCAPS" -> onEncaps(peer, msg);
                case "SECURE" -> onSecure(peer, msg);
                default -> log.warn("Unknown P2P message type from {}", ws.getId());
            }
        } catch (Exception e) {
            log.warn("Rejected P2P message ({}): {}", ws.getId(), e.getMessage());
        }
    }

    private void onHello(PeerSession peer, Map<String, Object> msg) {
        String nodeId = (String) msg.get("nodeId");
        String dsaPub = (String) msg.get("dsaPub");
        String kemPub = (String) msg.get("kemPub");
        String signature = (String) msg.get("signature");

        requireAuthentic(nodeId, dsaPub, kemPub + "|" + nodeId, signature);

        // Derive the one-time session key and send the signed ciphertext back.
        PqcKeyExchangeService.Encapsulation enc = keyExchange.encapsulate(kemPub);
        peer.peerNodeId(nodeId);
        peer.establish(enc.sessionKey());

        send(peer, Map.of(
                "type", "ENCAPS",
                "nodeId", wallet.address(),
                "dsaPub", wallet.publicKeyBase64(),
                "ciphertext", enc.ciphertextBase64(),
                "signature", wallet.sign(enc.ciphertextBase64() + "|" + wallet.address())));

        events.publishEvent(new LedgerEvent(LedgerEvent.Type.PEER_CONNECTED,
                Map.of("nodeId", nodeId, "direction", "inbound")));
        log.info("PQC channel established with inbound peer {}", nodeId);
    }

    private void onEncaps(PeerSession peer, Map<String, Object> msg) {
        String nodeId = (String) msg.get("nodeId");
        String dsaPub = (String) msg.get("dsaPub");
        String ciphertext = (String) msg.get("ciphertext");
        String signature = (String) msg.get("signature");

        requireAuthentic(nodeId, dsaPub, ciphertext + "|" + nodeId, signature);

        peer.peerNodeId(nodeId);
        peer.establish(keyExchange.decapsulate(ciphertext, peer.ephemeralKemPrivate()));

        events.publishEvent(new LedgerEvent(LedgerEvent.Type.PEER_CONNECTED,
                Map.of("nodeId", nodeId, "direction", "outbound")));
        log.info("PQC channel established with outbound peer {}", nodeId);

        // First act on a fresh secure channel: ask for the peer's ledger.
        sendSecure(peer, Map.of("type", "CHAIN_REQUEST"));
    }

    /**
     * Verifies the handshake signature AND that the claimed node id is the fingerprint
     * of the signing key — the identity binding that defeats key-substitution MITM.
     */
    private void requireAuthentic(String nodeId, String dsaPub, String signedContent,
                                  String signature) {
        if (!signatures.verify(signedContent, signature, dsaPub)) {
            throw new SecurityException("Handshake rejected: invalid ML-DSA signature");
        }
        try {
            if (!signatures.fingerprint(signatures.decodePublicKey(dsaPub)).equals(nodeId)) {
                throw new SecurityException("Handshake rejected: node id is not the key fingerprint");
            }
        } catch (java.security.GeneralSecurityException e) {
            throw new SecurityException("Handshake rejected: malformed ML-DSA key");
        }
    }

    // ------------------------------------------------------------------ secure traffic

    private void onSecure(PeerSession peer, Map<String, Object> msg) throws IOException {
        if (!peer.established()) {
            throw new SecurityException("SECURE frame before handshake completion");
        }
        // GCM decryption throws on any tampering — integrity and confidentiality in one step.
        String plaintext = channel.decrypt((String) msg.get("payload"), peer.sessionKey());
        Map<String, Object> inner = mapper.readValue(plaintext, new TypeReference<>() {
        });

        switch (String.valueOf(inner.get("type"))) {
            case "CHAIN_REQUEST" -> sendSecure(peer, Map.of(
                    "type", "CHAIN_RESPONSE",
                    "blocks", blockchain.blocks()));
            case "CHAIN_RESPONSE" -> {
                List<Block> remoteChain = mapper.convertValue(
                        inner.get("blocks"), new TypeReference<List<Block>>() {
                        });
                boolean adopted = blockchain.replaceChain(remoteChain);
                log.info("Chain from {}: length={} adopted={}", peer.peerNodeId(),
                        remoteChain.size(), adopted);
            }
            case "ANNOUNCE" -> {
                int remoteLength = ((Number) inner.get("length")).intValue();
                if (remoteLength > blockchain.length()) {
                    sendSecure(peer, Map.of("type", "CHAIN_REQUEST"));
                }
            }
            default -> log.warn("Unknown secure message from {}", peer.peerNodeId());
        }
    }

    /** Asks every established peer for its chain; longest valid chain wins locally. */
    public int syncWithPeers() {
        int asked = 0;
        for (PeerSession peer : peers.values()) {
            if (peer.established()) {
                sendSecure(peer, Map.of("type", "CHAIN_REQUEST"));
                asked++;
            }
        }
        return asked;
    }

    /** Gossip: whenever this node seals a block, tell peers so they can pull the longer chain. */
    @EventListener
    public void onLedgerEvent(LedgerEvent event) {
        if (event.type() != LedgerEvent.Type.BLOCK_ADDED) {
            return;
        }
        for (PeerSession peer : peers.values()) {
            if (peer.established()) {
                sendSecure(peer, Map.of("type", "ANNOUNCE", "length", blockchain.length()));
            }
        }
    }

    private void sendSecure(PeerSession peer, Map<String, Object> inner) {
        try {
            String plaintext = mapper.writeValueAsString(inner);
            send(peer, Map.of(
                    "type", "SECURE",
                    "payload", channel.encrypt(plaintext, peer.sessionKey())));
        } catch (IOException e) {
            log.warn("Failed to send secure frame to {}: {}", peer.peerNodeId(), e.getMessage());
        }
    }

    private void send(PeerSession peer, Map<String, Object> message) {
        try {
            String json = mapper.writeValueAsString(message);
            // WebSocketSession is not thread-safe for concurrent writes.
            synchronized (peer.ws()) {
                peer.ws().sendMessage(new TextMessage(json));
            }
        } catch (IOException e) {
            log.warn("Failed to send P2P frame: {}", e.getMessage());
        }
    }
}
