using LegalChain.Contracts;

namespace LegalChain.Tests;

/// <summary>
/// BE-10..BE-12: the medical consent registry is append-only and pseudonymous (grant and
/// revoke are both immutable events), and the agri supply chain enforces forward-only
/// stage progression — the document-fraud case a paper trail cannot prevent.
/// </summary>
public class ContractsTest
{
    [Fact]
    public void MedicalConsentGrantAndRevokeAreBothRecorded()
    {
        var medical = new MedicalConsentContract();
        ContractResult grant = medical.Execute(new Dictionary<string, string>
        {
            ["patientId"] = "patient-1",
            ["granteeId"] = "dr-who",
            ["scope"] = "IMAGING",
            ["granted"] = "true"
        });
        Assert.True(grant.Accepted);
        Assert.True(medical.HasConsent("patient-1", "dr-who", "IMAGING"));

        medical.Execute(new Dictionary<string, string>
        {
            ["patientId"] = "patient-1",
            ["granteeId"] = "dr-who",
            ["scope"] = "IMAGING",
            ["granted"] = "false"
        });
        Assert.False(medical.HasConsent("patient-1", "dr-who", "IMAGING"), "latest record wins");
        // Revocation appends — it never rewrites the grant.
        Assert.Equal(2, medical.HistoryOf("patient-1").Count);

        // BE-11: the on-chain record carries only pseudonymous ids/scope/decision.
        Assert.Equal(
            new[] { "contract", "granted", "granteeId", "patientId", "scope" },
            grant.Record.Keys.OrderBy(k => k, StringComparer.Ordinal).ToArray());
    }

    [Fact]
    public void MedicalConsentEnforcesClosedScopeVocabularyAndRequiredFields()
    {
        var medical = new MedicalConsentContract();
        Assert.Throws<ArgumentException>(() => medical.Execute(new Dictionary<string, string>
        {
            ["patientId"] = "p",
            ["granteeId"] = "g",
            ["scope"] = "TELEPATHY",
            ["granted"] = "true"
        }));
        Assert.Throws<ArgumentException>(() => medical.Execute(new Dictionary<string, string>
        {
            ["patientId"] = "p"
        }));
        Assert.Empty(medical.HistoryOf("p"));
    }

    [Fact]
    public void AgriTrailIsForwardOnlyFromHarvestToRetail()
    {
        var agri = new AgriSupplyChainContract();

        // Must start at HARVEST.
        Assert.Throws<ArgumentException>(() => agri.Execute(Event("B-1", "TRANSPORT")));

        agri.Execute(Event("B-1", "HARVEST"));
        agri.Execute(Event("B-1", "PROCESSING"));
        agri.Execute(Event("B-1", "TRANSPORT"));

        // Stage regression (paper fraud) is rejected; same-stage repeats are allowed.
        Assert.Throws<ArgumentException>(() => agri.Execute(Event("B-1", "PROCESSING")));
        agri.Execute(Event("B-1", "TRANSPORT"));

        Assert.Equal(["HARVEST", "PROCESSING", "TRANSPORT", "TRANSPORT"],
            agri.TrailOf("B-1").Select(e => e.Stage).ToList());
        Assert.Throws<ArgumentException>(() => agri.Execute(Event("B-1", "SPACE_ELEVATOR")));
    }

    private static Dictionary<string, string> Event(string batchId, string stage) => new()
    {
        ["batchId"] = batchId,
        ["stage"] = stage,
        ["actor"] = "Green Farm",
        ["location"] = "Lublin",
        ["details"] = "test"
    };
}
