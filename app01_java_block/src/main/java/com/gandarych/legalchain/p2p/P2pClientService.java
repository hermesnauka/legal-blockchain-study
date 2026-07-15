package com.gandarych.legalchain.p2p;

import jakarta.websocket.ContainerProvider;
import jakarta.websocket.WebSocketContainer;
import org.springframework.stereotype.Service;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.client.standard.StandardWebSocketClient;

import java.util.concurrent.TimeUnit;

/**
 * Outbound side of the P2P mesh: connects this node to a remote peer's
 * {@code /ws/p2p} endpoint. The connection attempt runs on a Loom virtual thread
 * (cheap blocking I/O), and the moment the socket opens, {@link P2pLinkHandler}
 * triggers the PQC handshake as initiator.
 */
@Service
public class P2pClientService {

    /** Chain frames carry ~7 KB per signed transaction (ML-DSA sizes) — allow large messages. */
    private static final int MAX_MESSAGE_BYTES = 10 * 1024 * 1024;

    private final P2pService p2p;

    public P2pClientService(P2pService p2p) {
        this.p2p = p2p;
    }

    /**
     * Connects to a peer, e.g. {@code ws://localhost:8081/ws/p2p}.
     *
     * @return the opened session (handshake continues asynchronously)
     */
    public WebSocketSession connect(String url) {
        try {
            WebSocketContainer container = ContainerProvider.getWebSocketContainer();
            container.setDefaultMaxTextMessageBufferSize(MAX_MESSAGE_BYTES);
            StandardWebSocketClient client = new StandardWebSocketClient(container);
            return client.execute(new P2pLinkHandler(p2p, true, url), url)
                    .get(10, TimeUnit.SECONDS);
        } catch (Exception e) {
            throw new IllegalArgumentException("Cannot connect to peer " + url + ": " + e.getMessage(), e);
        }
    }
}
