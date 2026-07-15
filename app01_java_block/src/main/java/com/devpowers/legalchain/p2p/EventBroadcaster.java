package com.devpowers.legalchain.p2p;

import com.devpowers.legalchain.core.LedgerEvent;
import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;
import org.springframework.web.socket.CloseStatus;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.handler.TextWebSocketHandler;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Frontend live feed at {@code /ws/events}: relays every {@link LedgerEvent} to all
 * connected browsers as {@code {"type": ..., "data": ...}} so the educational UI can
 * animate blocks, transactions and peer connections in real time. This channel carries
 * only data that is public on the ledger anyway, so unlike {@code /ws/p2p} it is not
 * encrypted at the application layer.
 */
@Component
public class EventBroadcaster extends TextWebSocketHandler {

    private static final Logger log = LoggerFactory.getLogger(EventBroadcaster.class);

    private final ObjectMapper mapper;
    private final Map<String, WebSocketSession> sessions = new ConcurrentHashMap<>();

    public EventBroadcaster(ObjectMapper mapper) {
        this.mapper = mapper;
    }

    @Override
    public void afterConnectionEstablished(WebSocketSession session) {
        sessions.put(session.getId(), session);
    }

    @Override
    public void afterConnectionClosed(WebSocketSession session, CloseStatus status) {
        sessions.remove(session.getId());
    }

    @EventListener
    public void onLedgerEvent(LedgerEvent event) {
        String json;
        try {
            json = mapper.writeValueAsString(Map.of("type", event.type(), "data", event.data()));
        } catch (Exception e) {
            log.warn("Cannot serialize ledger event: {}", e.getMessage());
            return;
        }
        sessions.values().forEach(session -> {
            try {
                synchronized (session) {
                    if (session.isOpen()) {
                        session.sendMessage(new TextMessage(json));
                    }
                }
            } catch (Exception e) {
                log.debug("Dropping dead event session {}", session.getId());
                sessions.remove(session.getId());
            }
        });
    }
}
