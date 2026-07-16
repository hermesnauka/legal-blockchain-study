using System.ComponentModel.DataAnnotations;
using LegalChain.Consensus;
using LegalChain.Core;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>Inspect and switch the active consensus strategy (Strategy pattern, live).</summary>
[ApiController]
[Route("/api/consensus")]
public class ConsensusController : ControllerBase
{
    private readonly ConsensusEngine _engine;
    private readonly EventBus _events;

    public ConsensusController(ConsensusEngine engine, EventBus events)
    {
        _engine = engine;
        _events = events;
    }

    [HttpGet]
    public object Consensus() => new
    {
        active = _engine.Active.Name,
        available = _engine.Available()
            .Select(s => new { name = s.Name, description = s.Description })
            .ToList()
    };

    [HttpPost]
    public object SwitchStrategy([FromBody] SwitchRequest request)
    {
        _engine.SwitchTo(request.Strategy);
        _events.Publish(new LedgerEvent(LedgerEvent.EventType.CONSENSUS_CHANGED,
            new Dictionary<string, string> { ["active"] = _engine.Active.Name }));
        return new { active = _engine.Active.Name };
    }

    public record SwitchRequest([Required(AllowEmptyStrings = false)] string Strategy);
}
