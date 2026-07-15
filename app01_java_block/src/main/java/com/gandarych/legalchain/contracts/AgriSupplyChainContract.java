package com.gandarych.legalchain.contracts;

import org.springframework.stereotype.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Agriculture-sector contract: farm-to-fork supply chain transparency.
 *
 * <p>Each batch of produce accumulates an ordered, immutable trail of stage events
 * (harvest → processing → transport → warehouse → retail). The contract enforces that
 * stages only move <b>forward</b> — a shipment cannot "return" to the farm on paper,
 * which is precisely the class of document fraud this eliminates. Because every event
 * is a signed ledger transaction, any consumer, importer or food-safety authority
 * (e.g. under EU regulation 178/2002 traceability requirements) can audit the full
 * provenance of a batch without trusting any single participant's database.</p>
 */
@Component
public class AgriSupplyChainContract implements SmartContract {

    /** Ordered lifecycle stages; index = position in the lifecycle. */
    public static final List<String> STAGES =
            List.of("HARVEST", "PROCESSING", "TRANSPORT", "WAREHOUSE", "RETAIL");

    private final Map<String, List<SupplyChainEvent>> batches = new ConcurrentHashMap<>();

    @Override
    public String contractId() {
        return "agri-supply-chain-v1";
    }

    @Override
    public String description() {
        return "Farm-to-fork traceability: forward-only, signed stage events per batch "
                + "(harvest → processing → transport → warehouse → retail).";
    }

    @Override
    public ContractResult execute(Map<String, String> input) {
        String batchId = require(input, "batchId");
        String stage = require(input, "stage");
        String actor = require(input, "actor");
        String location = require(input, "location");
        String details = input.getOrDefault("details", "");

        int stageIndex = STAGES.indexOf(stage);
        if (stageIndex < 0) {
            throw new IllegalArgumentException("Unknown stage: " + stage + " (allowed: " + STAGES + ")");
        }

        List<SupplyChainEvent> trail = batches.computeIfAbsent(batchId, k -> new ArrayList<>());
        if (!trail.isEmpty()) {
            int lastIndex = STAGES.indexOf(trail.getLast().stage());
            if (stageIndex < lastIndex) {
                throw new IllegalArgumentException("Stage regression rejected: batch " + batchId
                        + " is already at " + trail.getLast().stage() + ", cannot record " + stage);
            }
        } else if (stageIndex != 0) {
            throw new IllegalArgumentException("Batch " + batchId + " must start at HARVEST");
        }

        SupplyChainEvent event = new SupplyChainEvent(
                batchId, stage, actor, location, details, System.currentTimeMillis());
        trail.add(event);

        return new ContractResult(
                contractId(), true,
                "Batch " + batchId + " recorded at stage " + stage + " (" + location + ")",
                Map.of("contract", contractId(), "batchId", batchId, "stage", stage,
                        "actor", actor, "location", location, "details", details),
                null);
    }

    /** Full provenance trail of a batch — the consumer/auditor view. */
    public List<SupplyChainEvent> trailOf(String batchId) {
        return List.copyOf(batches.getOrDefault(batchId, List.of()));
    }

    private static String require(Map<String, String> input, String key) {
        String value = input.get(key);
        if (value == null || value.isBlank()) {
            throw new IllegalArgumentException("Missing contract input: " + key);
        }
        return value;
    }

    public record SupplyChainEvent(String batchId, String stage, String actor,
                                   String location, String details, long timestamp) {
    }
}
