namespace LegalChain.Core;

/// <summary>
/// Domain event published by the ledger through the in-process <see cref="EventBus"/>.
/// The WebSocket broadcaster relays these to the educational frontend at <c>/ws/events</c>
/// so the UI can animate blocks and transactions the moment they happen.
/// </summary>
/// <remarks>Member names are UPPER_SNAKE_CASE because they travel verbatim on the wire.</remarks>
public record LedgerEvent(LedgerEvent.EventType Type, object Data)
{
    public enum EventType
    {
        BLOCK_ADDED,
        TX_ADDED,
        PEER_CONNECTED,
        CHAIN_REPLACED,
        CONSENSUS_CHANGED
    }
}
