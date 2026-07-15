package com.devpowers.legalchain.contracts;

import org.junit.jupiter.api.Test;

import java.util.Map;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

/** Contract rule enforcement: revocable medical consent and forward-only agri stages. */
class ContractsTest {

    @Test
    void medicalConsentGrantAndRevokeIsAuditable() {
        MedicalConsentContract contract = new MedicalConsentContract();

        contract.execute(Map.of("patientId", "patient-1", "granteeId", "clinic-A",
                "scope", "LAB_RESULTS", "granted", "true"));
        assertThat(contract.hasConsent("patient-1", "clinic-A", "LAB_RESULTS")).isTrue();

        contract.execute(Map.of("patientId", "patient-1", "granteeId", "clinic-A",
                "scope", "LAB_RESULTS", "granted", "false"));
        assertThat(contract.hasConsent("patient-1", "clinic-A", "LAB_RESULTS")).isFalse();

        // Revocation does not erase history — the audit trail keeps both events.
        assertThat(contract.historyOf("patient-1")).hasSize(2);
    }

    @Test
    void medicalConsentRejectsUnknownScope() {
        MedicalConsentContract contract = new MedicalConsentContract();
        assertThatThrownBy(() -> contract.execute(Map.of(
                "patientId", "p", "granteeId", "g", "scope", "EVERYTHING", "granted", "true")))
                .isInstanceOf(IllegalArgumentException.class);
    }

    @Test
    void agriTrailEnforcesForwardOnlyStages() {
        AgriSupplyChainContract contract = new AgriSupplyChainContract();

        contract.execute(Map.of("batchId", "b-1", "stage", "HARVEST",
                "actor", "farm", "location", "Lublin"));
        contract.execute(Map.of("batchId", "b-1", "stage", "TRANSPORT",
                "actor", "carrier", "location", "Warszawa"));

        // Going back to PROCESSING after TRANSPORT is document fraud — rejected.
        assertThatThrownBy(() -> contract.execute(Map.of("batchId", "b-1", "stage", "PROCESSING",
                "actor", "x", "location", "y")))
                .isInstanceOf(IllegalArgumentException.class);

        assertThat(contract.trailOf("b-1")).hasSize(2);
    }

    @Test
    void agriBatchMustStartAtHarvest() {
        AgriSupplyChainContract contract = new AgriSupplyChainContract();
        assertThatThrownBy(() -> contract.execute(Map.of("batchId", "b-2", "stage", "RETAIL",
                "actor", "shop", "location", "Kraków")))
                .isInstanceOf(IllegalArgumentException.class);
    }
}
