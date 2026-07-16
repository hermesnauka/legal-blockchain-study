#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "contracts/SmartContract.h"

namespace legalchain::contracts {

/// Medical-sector contract: consent-based access to patient history.
///
/// How it stays GDPR-compliant on an immutable ledger: no medical data and no
/// real identity ever touches the chain. The contract records only
/// pseudonymous identifiers (patientId / granteeId are opaque strings — in
/// production, DID key fingerprints), a consent scope and a grant/revoke
/// flag. The chain thus becomes a tamper-proof consent audit trail: a
/// hospital can prove it held valid consent at the time of access (GDPR
/// art. 7 accountability), and revocation is itself an immutable event —
/// consent history can never be silently rewritten.
class MedicalConsentContract : public SmartContract {
public:
    struct ConsentRecord {
        std::string patientId;
        std::string granteeId;
        std::string scope;
        bool granted;
        int64_t timestampMs;
    };

    /// Allowed consent scopes — a closed vocabulary keeps records machine-auditable.
    static const std::vector<std::string>& scopes();

    std::string contractId() const override { return "medical-consent-v1"; }
    std::string description() const override;
    ContractResult execute(const std::map<std::string, std::string>& input) override;

    /// Full consent history for a patient — the audit trail view.
    std::vector<ConsentRecord> historyOf(const std::string& patientId) const;

    /// Access check used by hypothetical medical systems: latest record wins.
    bool hasConsent(const std::string& patientId, const std::string& granteeId, const std::string& scope) const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::vector<ConsentRecord>> consents_;

    static std::string require(const std::map<std::string, std::string>& input, const std::string& key);
};

} // namespace legalchain::contracts
