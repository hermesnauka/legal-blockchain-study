// Contract rule enforcement: revocable medical consent and forward-only agri
// stages. Mirrors app01_java_block's ContractsTest.java.
#include <gtest/gtest.h>

#include "contracts/AgriSupplyChainContract.h"
#include "contracts/MedicalConsentContract.h"

using namespace legalchain::contracts;

TEST(ContractsTest, MedicalConsentGrantAndRevokeIsAuditable) {
    MedicalConsentContract contract;

    contract.execute({{"patientId", "patient-1"}, {"granteeId", "clinic-A"}, {"scope", "LAB_RESULTS"},
                       {"granted", "true"}});
    EXPECT_TRUE(contract.hasConsent("patient-1", "clinic-A", "LAB_RESULTS"));

    contract.execute({{"patientId", "patient-1"}, {"granteeId", "clinic-A"}, {"scope", "LAB_RESULTS"},
                       {"granted", "false"}});
    EXPECT_FALSE(contract.hasConsent("patient-1", "clinic-A", "LAB_RESULTS"));

    // Revocation does not erase history — the audit trail keeps both events.
    EXPECT_EQ(contract.historyOf("patient-1").size(), 2u);
}

TEST(ContractsTest, MedicalConsentRejectsUnknownScope) {
    MedicalConsentContract contract;
    EXPECT_THROW(
        contract.execute({{"patientId", "p"}, {"granteeId", "g"}, {"scope", "EVERYTHING"}, {"granted", "true"}}),
        std::invalid_argument);
}

TEST(ContractsTest, AgriTrailEnforcesForwardOnlyStages) {
    AgriSupplyChainContract contract;

    contract.execute({{"batchId", "b-1"}, {"stage", "HARVEST"}, {"actor", "farm"}, {"location", "Lublin"}});
    contract.execute(
        {{"batchId", "b-1"}, {"stage", "TRANSPORT"}, {"actor", "carrier"}, {"location", "Warszawa"}});

    // Going back to PROCESSING after TRANSPORT is document fraud — rejected.
    EXPECT_THROW(
        contract.execute({{"batchId", "b-1"}, {"stage", "PROCESSING"}, {"actor", "x"}, {"location", "y"}}),
        std::invalid_argument);

    EXPECT_EQ(contract.trailOf("b-1").size(), 2u);
}

TEST(ContractsTest, AgriBatchMustStartAtHarvest) {
    AgriSupplyChainContract contract;
    EXPECT_THROW(
        contract.execute({{"batchId", "b-2"}, {"stage", "RETAIL"}, {"actor", "shop"}, {"location", "Krakow"}}),
        std::invalid_argument);
}
