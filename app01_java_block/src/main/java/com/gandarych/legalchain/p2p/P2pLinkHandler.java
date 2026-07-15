package com.gandarych.legalchain.p2p;

import org.springframework.web.socket.CloseStatus;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.handler.TextWebSocketHandler;

/**
 * Thin WebSocket adapter for a node-to-node link; all protocol logic lives in
 * {@link P2pService}. The same handler serves both roles: as the server endpoint
 * ({@code /ws/p2p}, initiator = false, waits for HELLO) and as the outbound client
 * handler (initiator = true, sends HELLO on connect).
 */
public class P2pLinkHandler extends TextWebSocketHandler {

    private final P2pService p2p;
    private final boolean initiator;
    private final String url;

    public P2pLinkHandler(P2pService p2p, boolean initiator, String url) {
        this.p2p = p2p;
        this.initiator = initiator;
        this.url = url;
    }

    @Override
    public void afterConnectionEstablished(WebSocketSession session) {
        p2p.register(session, initiator, url);
    }

    @Override
    protected void handleTextMessage(WebSocketSession session, TextMessage message) {
        p2p.onMessage(session, message.getPayload());
    }

    @Override
    public void afterConnectionClosed(WebSocketSession session, CloseStatus status) {
        p2p.unregister(session);
    }
}
