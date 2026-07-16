#include "core/TokenomicsService.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "core/IdGen.h"

namespace legalchain::core {

double TokenomicsService::rewardForBlock(int64_t blockIndex, double supplyAlreadyIssued) const {
    int64_t halvings = blockIndex / halvingInterval_;
    double reward = blockReward_ / std::pow(2.0, static_cast<double>(halvings));
    double remaining = maxSupply_ - supplyAlreadyIssued;
    return std::max(0.0, std::min(reward, remaining));
}

Transaction TokenomicsService::rewardTransaction(const std::string& validatorAddress, int64_t blockIndex,
                                                   double supplyAlreadyIssued) const {
    return Transaction(
        newUuid(), nowMillis(), Transaction::SYSTEM, validatorAddress,
        rewardForBlock(blockIndex, supplyAlreadyIssued), TransactionType::REWARD,
        std::map<std::string, std::string>{{"reason", "block-reward"}, {"block", std::to_string(blockIndex)}},
        std::nullopt, std::nullopt);
}

double TokenomicsService::issuedSupply(const std::vector<Block>& chain) {
    double total = 0.0;
    for (const auto& block : chain) {
        for (const auto& tx : block.transactions()) {
            if (tx.isReward()) {
                total += tx.amount();
            }
        }
    }
    return total;
}

} // namespace legalchain::core
