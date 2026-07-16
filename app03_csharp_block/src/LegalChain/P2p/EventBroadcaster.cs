using System.Collections.Concurrent;
using System.Net.WebSockets;
using System.Text.Json;
using LegalChain.Core;

namespace LegalChain.P2p;

/// <summary>
/// Frontend live feed at <c>/ws/events</c>: relays every <see cref="LedgerEvent"/> to all
/// connected browsers as <c>{"type": ..., "data": ...}</c> so the educational UI can
/// animate blocks, transactions and peer connections in real time. This channel carries
/// only data that is public on the ledger anyway, so unlike <c>/ws/p2p</c> it is not
/// encrypted at the application layer.
/// </summary>
public class EventBroadcaster
{
    private readonly ILogger<EventBroadcaster> _log;
    private readonly ConcurrentDictionary<string, (WebSocket Socket, SemaphoreSlim SendLock)> _sessions = new();

    public EventBroadcaster(EventBus events, ILogger<EventBroadcaster> log)
    {
        _log = log;
        events.Subscribe(OnLedgerEvent);
    }

    /// <summary>
    /// Attaches a browser socket and blocks until the client disconnects (the returned
    /// task completes when the socket closes).
    /// </summary>
    public async Task AttachAsync(WebSocket socket, CancellationToken ct = default)
    {
        string id = Guid.NewGuid().ToString();
        _sessions[id] = (socket, new SemaphoreSlim(1, 1));
        var buffer = new byte[4 * 1024];
        try
        {
            // Browsers only listen on this feed; the receive loop exists to observe the close.
            while (socket.State == WebSocketState.Open && !ct.IsCancellationRequested)
            {
                WebSocketReceiveResult result = await socket.ReceiveAsync(buffer, ct);
                if (result.MessageType == WebSocketMessageType.Close)
                {
                    break;
                }
            }
        }
        catch (Exception e) when (e is WebSocketException or OperationCanceledException)
        {
            _log.LogDebug("Event feed client closed: {Message}", e.Message);
        }
        finally
        {
            _sessions.TryRemove(id, out _);
        }
    }

    private void OnLedgerEvent(LedgerEvent ledgerEvent)
    {
        byte[] json;
        try
        {
            json = JsonSerializer.SerializeToUtf8Bytes(new Dictionary<string, object?>
            {
                ["type"] = ledgerEvent.Type.ToString(),
                ["data"] = ledgerEvent.Data
            }, JsonDefaults.Options);
        }
        catch (Exception e)
        {
            _log.LogWarning("Cannot serialize ledger event: {Message}", e.Message);
            return;
        }
        foreach (var (id, session) in _sessions)
        {
            // Fire-and-forget per client: a slow browser must never stall the ledger.
            _ = SendAsync(id, session, json);
        }
    }

    private async Task SendAsync(string id, (WebSocket Socket, SemaphoreSlim SendLock) session, byte[] json)
    {
        await session.SendLock.WaitAsync();
        try
        {
            if (session.Socket.State == WebSocketState.Open)
            {
                await session.Socket.SendAsync(json, WebSocketMessageType.Text, endOfMessage: true,
                    CancellationToken.None);
            }
        }
        catch (Exception)
        {
            _log.LogDebug("Dropping dead event session {Id}", id);
            _sessions.TryRemove(id, out _);
        }
        finally
        {
            session.SendLock.Release();
        }
    }
}
