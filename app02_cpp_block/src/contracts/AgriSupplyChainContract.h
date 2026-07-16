#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "contracts/SmartContract.h"

namespace legalchain::contracts {

/// Agriculture-sector contract: farm-to-fork supply chain transparency.
///
/// Each batch of produce accumulates an ordered, immutable trail of stage
/// events (harvest -> processing -> transport -> warehouse -> retail). The
/// contract enforces that stages only move forward — a shipment cannot
/// "return" to the farm on paper, which is precisely the class of document
/// fraud this eliminates. Because every event is a signed ledger
/// transaction, any consumer, importer or food-safety authority (e.g. under
/// EU regulation 178/2002 traceability requirements) can audit the full
/// provenance of a batch without trusting any single participant's database.
class AgriSupplyChainContract : public SmartContract {
public:
    struct SupplyChainEvent {
        std::string batchId;
        std::string stage;
        std::string actor;
        std::string location;
        std::string details;
        int64_t timestampMs;
    };

    /// Ordered lifecycle stages; index = position in the lifecycle.
    static const std::vector<std::string>& stages();

    std::string contractId() const override { return "agri-supply-chain-v1"; }
    std::string description() const override;
    ContractResult execute(const std::map<std::string, std::string>& input) override;

    /// Full provenance trail of a batch — the consumer/auditor view.
    std::vector<SupplyChainEvent> trailOf(const std::string& batchId) const;

private:
    mutable std::mutex mutex_;
    std::map<std::string, std::vector<SupplyChainEvent>> batches_;

    static std::string require(const std::map<std::string, std::string>& input, const std::string& key);
    static int stageIndex(const std::string& stage);
};

} // namespace legalchain::contracts
