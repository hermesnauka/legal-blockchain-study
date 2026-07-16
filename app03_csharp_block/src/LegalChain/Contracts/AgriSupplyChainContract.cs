namespace LegalChain.Contracts;

/// <summary>
/// Agriculture-sector contract: farm-to-fork supply chain transparency.
/// </summary>
/// <remarks>
/// Each batch of produce accumulates an ordered, immutable trail of stage events
/// (harvest → processing → transport → warehouse → retail). The contract enforces that
/// stages only move <b>forward</b> — a shipment cannot "return" to the farm on paper,
/// which is precisely the class of document fraud this eliminates. Because every event
/// is a signed ledger transaction, any consumer, importer or food-safety authority
/// (e.g. under EU regulation 178/2002 traceability requirements) can audit the full
/// provenance of a batch without trusting any single participant's database.
/// </remarks>
public class AgriSupplyChainContract : ISmartContract
{
    /// <summary>Ordered lifecycle stages; index = position in the lifecycle.</summary>
    public static readonly IReadOnlyList<string> Stages =
        ["HARVEST", "PROCESSING", "TRANSPORT", "WAREHOUSE", "RETAIL"];

    private readonly Dictionary<string, List<SupplyChainEvent>> _batches = new();
    private readonly Lock _gate = new();

    public string ContractId => "agri-supply-chain-v1";

    public string Description
        => "Farm-to-fork traceability: forward-only, signed stage events per batch "
           + "(harvest → processing → transport → warehouse → retail).";

    public ContractResult Execute(IReadOnlyDictionary<string, string> input)
    {
        string batchId = Require(input, "batchId");
        string stage = Require(input, "stage");
        string actor = Require(input, "actor");
        string location = Require(input, "location");
        string details = input.GetValueOrDefault("details", "");

        int stageIndex = Stages.ToList().IndexOf(stage);
        if (stageIndex < 0)
        {
            throw new ArgumentException("Unknown stage: " + stage
                + " (allowed: [" + string.Join(", ", Stages) + "])");
        }

        SupplyChainEvent supplyEvent;
        lock (_gate)
        {
            if (!_batches.TryGetValue(batchId, out var trail))
            {
                _batches[batchId] = trail = new List<SupplyChainEvent>();
            }
            if (trail.Count > 0)
            {
                int lastIndex = Stages.ToList().IndexOf(trail[^1].Stage);
                if (stageIndex < lastIndex)
                {
                    throw new ArgumentException("Stage regression rejected: batch " + batchId
                        + " is already at " + trail[^1].Stage + ", cannot record " + stage);
                }
            }
            else if (stageIndex != 0)
            {
                throw new ArgumentException("Batch " + batchId + " must start at HARVEST");
            }

            supplyEvent = new SupplyChainEvent(
                batchId, stage, actor, location, details, DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
            trail.Add(supplyEvent);
        }

        return new ContractResult(
            ContractId, true,
            "Batch " + batchId + " recorded at stage " + stage + " (" + location + ")",
            new Dictionary<string, string>
            {
                ["contract"] = ContractId,
                ["batchId"] = batchId,
                ["stage"] = stage,
                ["actor"] = actor,
                ["location"] = location,
                ["details"] = details
            },
            null);
    }

    /// <summary>Full provenance trail of a batch — the consumer/auditor view.</summary>
    public IReadOnlyList<SupplyChainEvent> TrailOf(string batchId)
    {
        lock (_gate)
        {
            return _batches.TryGetValue(batchId, out var trail)
                ? trail.ToList()
                : [];
        }
    }

    private static string Require(IReadOnlyDictionary<string, string> input, string key)
    {
        if (!input.TryGetValue(key, out var value) || string.IsNullOrWhiteSpace(value))
        {
            throw new ArgumentException("Missing contract input: " + key);
        }
        return value;
    }

    public record SupplyChainEvent(string BatchId, string Stage, string Actor,
        string Location, string Details, long Timestamp);
}
