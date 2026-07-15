package com.gandarych.legalchain.config;

import com.gandarych.legalchain.p2p.EventBroadcaster;
import com.gandarych.legalchain.p2p.P2pLinkHandler;
import com.gandarych.legalchain.p2p.P2pService;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.socket.config.annotation.EnableWebSocket;
import org.springframework.web.socket.config.annotation.WebSocketConfigurer;
import org.springframework.web.socket.config.annotation.WebSocketHandlerRegistry;
import org.springframework.web.socket.server.standard.ServletServerContainerFactoryBean;

/**
 * WebSocket endpoints: {@code /ws/p2p} (node-to-node, PQC-encrypted protocol) and
 * {@code /ws/events} (frontend live feed). Message buffers are raised because a single
 * CHAIN_RESPONSE frame carries ML-DSA keys (~2.6 KB) and signatures (~4.4 KB) per
 * transaction — post-quantum security costs bandwidth, a trade-off the educational
 * frontend explicitly teaches.
 */
@Configuration
@EnableWebSocket
public class WebSocketConfig implements WebSocketConfigurer {

    private static final int MAX_MESSAGE_BYTES = 10 * 1024 * 1024;

    private final P2pService p2pService;
    private final EventBroadcaster eventBroadcaster;

    public WebSocketConfig(P2pService p2pService, EventBroadcaster eventBroadcaster) {
        this.p2pService = p2pService;
        this.eventBroadcaster = eventBroadcaster;
    }

    @Override
    public void registerWebSocketHandlers(WebSocketHandlerRegistry registry) {
        registry.addHandler(new P2pLinkHandler(p2pService, false, null), "/ws/p2p")
                .setAllowedOrigins("*");
        registry.addHandler(eventBroadcaster, "/ws/events")
                .setAllowedOrigins("*");
    }

    @Bean
    public ServletServerContainerFactoryBean websocketContainer() {
        ServletServerContainerFactoryBean container = new ServletServerContainerFactoryBean();
        container.setMaxTextMessageBufferSize(MAX_MESSAGE_BYTES);
        container.setMaxBinaryMessageBufferSize(MAX_MESSAGE_BYTES);
        return container;
    }
}
