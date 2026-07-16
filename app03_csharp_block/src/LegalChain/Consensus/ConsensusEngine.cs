namespace LegalChain.Consensus;

/// <summary>
/// Holds the registry of <see cref="IConsensusStrategy"/> implementations and the
/// currently active one. The ASP.NET Core container injects every registered strategy,
/// so a new consensus algorithm becomes available API-wide by simply registering one
/// more implementation of the interface.
/// </summary>
public class ConsensusEngine
{
    private readonly Dictionary<string, IConsensusStrategy> _strategies;
    private volatile IConsensusStrategy _active;

    public ConsensusEngine(IEnumerable<IConsensusStrategy> available, string defaultStrategy)
    {
        _strategies = available.ToDictionary(s => s.Name);
        _active = Require(defaultStrategy);
    }

    public IConsensusStrategy Active => _active;

    public IConsensusStrategy ByName(string name) => Require(name);

    public void SwitchTo(string name) => _active = Require(name);

    public IReadOnlyList<IConsensusStrategy> Available()
        => _strategies.Values.OrderBy(s => s.Name, StringComparer.Ordinal).ToList();

    private IConsensusStrategy Require(string name)
    {
        if (!_strategies.TryGetValue(name.ToUpperInvariant(), out var strategy))
        {
            throw new ArgumentException("Unknown consensus strategy: " + name
                + " (available: [" + string.Join(", ", _strategies.Keys) + "])");
        }
        return strategy;
    }
}
