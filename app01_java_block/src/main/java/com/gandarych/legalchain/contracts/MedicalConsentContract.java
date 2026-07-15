package com.gandarych.legalchain.contracts;

import org.springframework.stereotype.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Medical-sector contract: consent-based access to patient history.
 *
 * <p>How it stays GDPR-compliant on an immutable ledger: no medical data and no real
 * identity ever touches the chain. The contract records only pseudonymous identifiers
 * (patientId / granteeId are opaque strings — in production, DID key fingerprints), a
 * consent <i>scope</i> and a grant/revoke flag. The chain thus becomes a tamper-proof
 * consent audit trail: a hospital can prove it held valid consent at the time of
 * access (GDPR art. 7 accountability), and revocation is itself an immutable event —
 * consent history can never be silently rewritten.</p>
 */
@Component
public class MedicalConsentContract implements SmartContract {

    /** Allowed consent scopes — a closed vocabulary keeps records machine-auditable. */
    public static final List<String> SCOPES =
            List.of("HISTORY_READ", "LAB_RESULTS", "IMAGING", "PRESCRIPTIONS", "FULL_RECORD");

    /** Current consent state per patient (rebuilt from chain in a production system). */
    private final Map<String, List<ConsentRecord>> consents = new ConcurrentHashMap<>();

    @Override
    public String contractId() {
        return "medical-consent-v1";
    }

    @Override
    public String description() {
        return "Consent-based patient history access: pseudonymous, revocable, auditable "
                + "consent records (GDPR-compatible: no personal/medical data on-chain).";
    }

    @Override
    public ContractResult execute(Map<String, String> input) {
        String patientId = require(input, "patientId");
        String granteeId = require(input, "granteeId");
        String scope = require(input, "scope");
        boolean granted = Boolean.parseBoolean(require(input, "granted"));

        if (!SCOPES.contains(scope)) {
            throw new IllegalArgumentException("Unknown consent scope: " + scope
                    + " (allowed: " + SCOPES + ")");
        }

        ConsentRecord record = new ConsentRecord(
                patientId, granteeId, scope, granted, System.currentTimeMillis());
        consents.computeIfAbsent(patientId, k -> new ArrayList<>()).add(record);

        String action = granted ? "granted to" : "revoked from";
        return new ContractResult(
                contractId(), true,
                "Consent for scope " + scope + " " + action + " " + granteeId,
                Map.of("contract", contractId(), "patientId", patientId,
                        "granteeId", granteeId, "scope", scope,
                        "granted", String.valueOf(granted)),
                null);
    }

    /** Full consent history for a patient — the audit trail view. */
    public List<ConsentRecord> historyOf(String patientId) {
        return List.copyOf(consents.getOrDefault(patientId, List.of()));
    }

    /** Access check used by hypothetical medical systems: latest record wins. */
    public boolean hasConsent(String patientId, String granteeId, String scope) {
        List<ConsentRecord> history = consents.getOrDefault(patientId, List.of());
        return history.reversed().stream()
                .filter(r -> r.granteeId().equals(granteeId) && r.scope().equals(scope))
                .findFirst()
                .map(ConsentRecord::granted)
                .orElse(false);
    }

    private static String require(Map<String, String> input, String key) {
        String value = input.get(key);
        if (value == null || value.isBlank()) {
            throw new IllegalArgumentException("Missing contract input: " + key);
        }
        return value;
    }

    public record ConsentRecord(String patientId, String granteeId, String scope,
                                boolean granted, long timestamp) {
    }
}
