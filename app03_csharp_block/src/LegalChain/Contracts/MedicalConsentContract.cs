namespace LegalChain.Contracts;

/// <summary>
/// Medical-sector contract: consent-based access to patient history.
/// </summary>
/// <remarks>
/// How it stays GDPR-compliant on an immutable ledger: no medical data and no real
/// identity ever touches the chain. The contract records only pseudonymous identifiers
/// (patientId / granteeId are opaque strings — in production, DID key fingerprints), a
/// consent <i>scope</i> and a grant/revoke flag. The chain thus becomes a tamper-proof
/// consent audit trail: a hospital can prove it held valid consent at the time of
/// access (GDPR art. 7 accountability), and revocation is itself an immutable event —
/// consent history can never be silently rewritten.
/// </remarks>
public class MedicalConsentContract : ISmartContract
{
    /// <summary>Allowed consent scopes — a closed vocabulary keeps records machine-auditable.</summary>
    public static readonly IReadOnlyList<string> Scopes =
        ["HISTORY_READ", "LAB_RESULTS", "IMAGING", "PRESCRIPTIONS", "FULL_RECORD"];

    /// <summary>Current consent state per patient (rebuilt from chain in a production system).</summary>
    private readonly Dictionary<string, List<ConsentRecord>> _consents = new();
    private readonly Lock _gate = new();

    public string ContractId => "medical-consent-v1";

    public string Description
        => "Consent-based patient history access: pseudonymous, revocable, auditable "
           + "consent records (GDPR-compatible: no personal/medical data on-chain).";

    public ContractResult Execute(IReadOnlyDictionary<string, string> input)
    {
        string patientId = Require(input, "patientId");
        string granteeId = Require(input, "granteeId");
        string scope = Require(input, "scope");
        bool granted = bool.Parse(Require(input, "granted"));

        if (!Scopes.Contains(scope))
        {
            throw new ArgumentException("Unknown consent scope: " + scope
                + " (allowed: [" + string.Join(", ", Scopes) + "])");
        }

        var record = new ConsentRecord(
            patientId, granteeId, scope, granted, DateTimeOffset.UtcNow.ToUnixTimeMilliseconds());
        lock (_gate)
        {
            if (!_consents.TryGetValue(patientId, out var history))
            {
                _consents[patientId] = history = new List<ConsentRecord>();
            }
            history.Add(record);
        }

        string action = granted ? "granted to" : "revoked from";
        return new ContractResult(
            ContractId, true,
            "Consent for scope " + scope + " " + action + " " + granteeId,
            new Dictionary<string, string>
            {
                ["contract"] = ContractId,
                ["patientId"] = patientId,
                ["granteeId"] = granteeId,
                ["scope"] = scope,
                ["granted"] = granted.ToString().ToLowerInvariant()
            },
            null);
    }

    /// <summary>Full consent history for a patient — the audit trail view.</summary>
    public IReadOnlyList<ConsentRecord> HistoryOf(string patientId)
    {
        lock (_gate)
        {
            return _consents.TryGetValue(patientId, out var history)
                ? history.ToList()
                : [];
        }
    }

    /// <summary>Access check used by hypothetical medical systems: latest record wins.</summary>
    public bool HasConsent(string patientId, string granteeId, string scope)
    {
        lock (_gate)
        {
            if (!_consents.TryGetValue(patientId, out var history))
            {
                return false;
            }
            for (int i = history.Count - 1; i >= 0; i--)
            {
                if (history[i].GranteeId == granteeId && history[i].Scope == scope)
                {
                    return history[i].Granted;
                }
            }
            return false;
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

    public record ConsentRecord(string PatientId, string GranteeId, string Scope,
        bool Granted, long Timestamp);
}
