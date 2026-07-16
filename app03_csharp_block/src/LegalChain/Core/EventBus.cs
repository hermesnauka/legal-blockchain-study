namespace LegalChain.Core;

/// <summary>
/// Minimal in-process publish/subscribe bus — the C# analogue of Spring's
/// <c>ApplicationEventPublisher</c> used by the Java node. Subscribers (the
/// <c>/ws/events</c> broadcaster and the P2P gossip) are registered at startup;
/// publication is synchronous and exceptions in one subscriber never break the ledger
/// operation that raised the event.
/// </summary>
public class EventBus
{
    private readonly List<Action<LedgerEvent>> _subscribers = new();
    private readonly Lock _gate = new();

    public void Subscribe(Action<LedgerEvent> subscriber)
    {
        lock (_gate)
        {
            _subscribers.Add(subscriber);
        }
    }

    public void Publish(LedgerEvent ledgerEvent)
    {
        List<Action<LedgerEvent>> snapshot;
        lock (_gate)
        {
            snapshot = new List<Action<LedgerEvent>>(_subscribers);
        }
        foreach (var subscriber in snapshot)
        {
            try
            {
                subscriber(ledgerEvent);
            }
            catch
            {
                // A failing observer (e.g. a dead WebSocket) must never break ledger writes.
            }
        }
    }
}
