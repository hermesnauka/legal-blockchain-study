#include "contracts/AgriSupplyChainContract.h"

#include <algorithm>
#include <stdexcept>

#include "core/IdGen.h"

namespace legalchain::contracts {

const std::vector<std::string>& AgriSupplyChainContract::stages() {
    static const std::vector<std::string> STAGES = {"HARVEST", "PROCESSING", "TRANSPORT", "WAREHOUSE", "RETAIL"};
    return STAGES;
}

int AgriSupplyChainContract::stageIndex(const std::string& stage) {
    const auto& all = stages();
    auto it = std::find(all.begin(), all.end(), stage);
    return it == all.end() ? -1 : static_cast<int>(std::distance(all.begin(), it));
}

std::string AgriSupplyChainContract::description() const {
    return "Farm-to-fork traceability: forward-only, signed stage events per batch "
           "(harvest -> processing -> transport -> warehouse -> retail).";
}

std::string AgriSupplyChainContract::require(const std::map<std::string, std::string>& input,
                                              const std::string& key) {
    auto it = input.find(key);
    if (it == input.end() || it->second.empty()) {
        throw std::invalid_argument("Missing contract input: " + key);
    }
    return it->second;
}

ContractResult AgriSupplyChainContract::execute(const std::map<std::string, std::string>& input) {
    std::string batchId = require(input, "batchId");
    std::string stage = require(input, "stage");
    std::string actor = require(input, "actor");
    std::string location = require(input, "location");
    std::string details;
    auto detailsIt = input.find("details");
    if (detailsIt != input.end()) details = detailsIt->second;

    int index = stageIndex(stage);
    if (index < 0) {
        throw std::invalid_argument("Unknown stage: " + stage);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto& trail = batches_[batchId];
    if (!trail.empty()) {
        int lastIndex = stageIndex(trail.back().stage);
        if (index < lastIndex) {
            throw std::invalid_argument("Stage regression rejected: batch " + batchId + " is already at "
                                         + trail.back().stage + ", cannot record " + stage);
        }
    } else if (index != 0) {
        throw std::invalid_argument("Batch " + batchId + " must start at HARVEST");
    }

    SupplyChainEvent event{batchId, stage, actor, location, details, core::nowMillis()};
    trail.push_back(event);

    ContractResult result;
    result.contractId = contractId();
    result.accepted = true;
    result.message = "Batch " + batchId + " recorded at stage " + stage + " (" + location + ")";
    result.record = {{"contract", contractId()}, {"batchId", batchId},   {"stage", stage},
                      {"actor", actor},           {"location", location}, {"details", details}};
    return result;
}

std::vector<AgriSupplyChainContract::SupplyChainEvent> AgriSupplyChainContract::trailOf(
    const std::string& batchId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = batches_.find(batchId);
    return it == batches_.end() ? std::vector<SupplyChainEvent>{} : it->second;
}

} // namespace legalchain::contracts
