using System.ComponentModel.DataAnnotations;
using LegalChain.Contracts;
using LegalChain.Core;
using Microsoft.AspNetCore.Mvc;

namespace LegalChain.Api;

/// <summary>Sector smart contracts: medical consent registry and agri supply-chain traceability.</summary>
[ApiController]
[Route("/api/contracts")]
public class ContractController : ControllerBase
{
    private readonly ContractEngine _engine;
    private readonly MedicalConsentContract _medical;
    private readonly AgriSupplyChainContract _agri;

    public ContractController(ContractEngine engine, MedicalConsentContract medical,
        AgriSupplyChainContract agri)
    {
        _engine = engine;
        _medical = medical;
        _agri = agri;
    }

    [HttpPost("medical/consent")]
    public ContractResult MedicalConsent([FromBody] MedicalConsentRequest request)
        => _engine.Execute(_medical, TransactionType.CONTRACT_MEDICAL, new Dictionary<string, string>
        {
            ["patientId"] = request.PatientId,
            ["granteeId"] = request.GranteeId,
            ["scope"] = request.Scope,
            ["granted"] = request.Granted.ToString().ToLowerInvariant()
        });

    [HttpGet("medical/{patientId}")]
    public IReadOnlyList<MedicalConsentContract.ConsentRecord> MedicalHistory(string patientId)
        => _medical.HistoryOf(patientId);

    [HttpPost("agri/event")]
    public ContractResult AgriEvent([FromBody] AgriEventRequest request)
        => _engine.Execute(_agri, TransactionType.CONTRACT_AGRI, new Dictionary<string, string>
        {
            ["batchId"] = request.BatchId,
            ["stage"] = request.Stage,
            ["actor"] = request.Actor,
            ["location"] = request.Location,
            ["details"] = request.Details ?? ""
        });

    [HttpGet("agri/{batchId}")]
    public IReadOnlyList<AgriSupplyChainContract.SupplyChainEvent> AgriTrail(string batchId)
        => _agri.TrailOf(batchId);

    public record MedicalConsentRequest(
        [property: Required(AllowEmptyStrings = false)] string PatientId,
        [property: Required(AllowEmptyStrings = false)] string GranteeId,
        [property: Required(AllowEmptyStrings = false)] string Scope,
        [property: Required] bool Granted);

    public record AgriEventRequest(
        [property: Required(AllowEmptyStrings = false)] string BatchId,
        [property: Required(AllowEmptyStrings = false)] string Stage,
        [property: Required(AllowEmptyStrings = false)] string Actor,
        [property: Required(AllowEmptyStrings = false)] string Location,
        string? Details);
}
