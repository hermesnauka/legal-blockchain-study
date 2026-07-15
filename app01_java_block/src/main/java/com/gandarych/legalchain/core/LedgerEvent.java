package com.gandarych.legalchain.core;

/**
 * Domain event published by the ledger (Spring application event). The WebSocket
 * broadcaster relays these to the educational frontend at {@code /ws/events} so the UI
 * can animate blocks and transactions the moment they happen.
 */
public record LedgerEvent(Type type, Object data) {

    public enum Type {
        BLOCK_ADDED,
        TX_ADDED,
        PEER_CONNECTED,
        CHAIN_REPLACED,
        CONSENSUS_CHANGED
    }
}
