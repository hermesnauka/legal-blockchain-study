namespace LegalChain.Config;

/// <summary>
/// Node configuration bound from the <c>LegalChain</c> section of appsettings.json,
/// overridable per node via CLI switches (<c>--port</c>, <c>--node-name</c>) — the same
/// shape used by the Java (application.yml) and C++ (application.json) nodes.
/// </summary>
public class LegalChainOptions
{
    public const string SectionName = "LegalChain";

    public int Port { get; set; } = 8100;

    /// <summary>Human-readable node name; the cryptographic node identity is the
    /// ML-DSA public-key fingerprint generated at startup (see <c>NodeWallet</c>).</summary>
    public string NodeName { get; set; } = "node-A";

    public ConsensusOptions Consensus { get; set; } = new();

    public TokenomicsOptions Tokenomics { get; set; } = new();

    public class ConsensusOptions
    {
        public string DefaultStrategy { get; set; } = "POS";
    }

    /// <summary>Educational tokenomics: fixed block reward, halved every
    /// <see cref="HalvingInterval"/> blocks, with a hard supply cap.</summary>
    public class TokenomicsOptions
    {
        public decimal BlockReward { get; set; } = 50m;
        public int HalvingInterval { get; set; } = 100;
        public decimal MaxSupply { get; set; } = 21000m;
    }
}
