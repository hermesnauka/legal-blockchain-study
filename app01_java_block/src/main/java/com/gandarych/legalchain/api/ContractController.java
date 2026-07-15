package com.gandarych.legalchain.api;

import com.gandarych.legalchain.contracts.AgriSupplyChainContract;
import com.gandarych.legalchain.contracts.ContractEngine;
import com.gandarych.legalchain.contracts.ContractResult;
import com.gandarych.legalchain.contracts.MedicalConsentContract;
import com.gandarych.legalchain.core.TransactionType;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Sector smart contracts: medical consent registry and agri supply-chain traceability. */
@RestController
@RequestMapping("/api/contracts")
public class ContractController {

    private final ContractEngine engine;
    private final MedicalConsentContract medical;
    private final AgriSupplyChainContract agri;

    public ContractController(ContractEngine engine, MedicalConsentContract medical,
                              AgriSupplyChainContract agri) {
        this.engine = engine;
        this.medical = medical;
        this.agri = agri;
    }

    @PostMapping("/medical/consent")
    public ContractResult medicalConsent(@Valid @RequestBody MedicalConsentRequest request) {
        return engine.execute(medical, TransactionType.CONTRACT_MEDICAL, Map.of(
                "patientId", request.patientId(),
                "granteeId", request.granteeId(),
                "scope", request.scope(),
                "granted", String.valueOf(request.granted())));
    }

    @GetMapping("/medical/{patientId}")
    public List<MedicalConsentContract.ConsentRecord> medicalHistory(@PathVariable String patientId) {
        return medical.historyOf(patientId);
    }

    @PostMapping("/agri/event")
    public ContractResult agriEvent(@Valid @RequestBody AgriEventRequest request) {
        Map<String, String> input = new HashMap<>();
        input.put("batchId", request.batchId());
        input.put("stage", request.stage());
        input.put("actor", request.actor());
        input.put("location", request.location());
        input.put("details", request.details() == null ? "" : request.details());
        return engine.execute(agri, TransactionType.CONTRACT_AGRI, input);
    }

    @GetMapping("/agri/{batchId}")
    public List<AgriSupplyChainContract.SupplyChainEvent> agriTrail(@PathVariable String batchId) {
        return agri.trailOf(batchId);
    }

    public record MedicalConsentRequest(
            @NotBlank String patientId,
            @NotBlank String granteeId,
            @NotBlank String scope,
            @NotNull Boolean granted) {
    }

    public record AgriEventRequest(
            @NotBlank String batchId,
            @NotBlank String stage,
            @NotBlank String actor,
            @NotBlank String location,
            String details) {
    }
}
