#include "contracts/MedicalConsentContract.h"

#include <algorithm>
#include <stdexcept>

#include "core/IdGen.h"

namespace legalchain::contracts {

const std::vector<std::string>& MedicalConsentContract::scopes() {
    static const std::vector<std::string> SCOPES = {"HISTORY_READ", "LAB_RESULTS", "IMAGING", "PRESCRIPTIONS",
                                                      "FULL_RECORD"};
    return SCOPES;
}

std::string MedicalConsentContract::description() const {
    return "Consent-based patient history access: pseudonymous, revocable, auditable "
           "consent records (GDPR-compatible: no personal/medical data on-chain).";
}

std::string MedicalConsentContract::require(const std::map<std::string, std::string>& input,
                                             const std::string& key) {
    auto it = input.find(key);
    if (it == input.end() || it->second.empty()) {
        throw std::invalid_argument("Missing contract input: " + key);
    }
    return it->second;
}

ContractResult MedicalConsentContract::execute(const std::map<std::string, std::string>& input) {
    std::string patientId = require(input, "patientId");
    std::string granteeId = require(input, "granteeId");
    std::string scope = require(input, "scope");
    std::string grantedStr = require(input, "granted");
    bool granted = grantedStr == "true";

    const auto& allowed = scopes();
    if (std::find(allowed.begin(), allowed.end(), scope) == allowed.end()) {
        throw std::invalid_argument("Unknown consent scope: " + scope);
    }

    ConsentRecord record{patientId, granteeId, scope, granted, core::nowMillis()};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        consents_[patientId].push_back(record);
    }

    std::string action = granted ? "granted to" : "revoked from";
    ContractResult result;
    result.contractId = contractId();
    result.accepted = true;
    result.message = "Consent for scope " + scope + " " + action + " " + granteeId;
    result.record = {{"contract", contractId()},
                      {"patientId", patientId},
                      {"granteeId", granteeId},
                      {"scope", scope},
                      {"granted", granted ? "true" : "false"}};
    return result;
}

std::vector<MedicalConsentContract::ConsentRecord> MedicalConsentContract::historyOf(
    const std::string& patientId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = consents_.find(patientId);
    return it == consents_.end() ? std::vector<ConsentRecord>{} : it->second;
}

bool MedicalConsentContract::hasConsent(const std::string& patientId, const std::string& granteeId,
                                         const std::string& scope) const {
    auto history = historyOf(patientId);
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (it->granteeId == granteeId && it->scope == scope) {
            return it->granted;
        }
    }
    return false;
}

} // namespace legalchain::contracts
