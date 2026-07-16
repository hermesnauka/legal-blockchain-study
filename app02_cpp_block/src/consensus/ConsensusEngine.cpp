#include "consensus/ConsensusEngine.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace legalchain::consensus {

std::string ConsensusEngine::toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::toupper(c); });
    return out;
}

ConsensusEngine::ConsensusEngine(std::vector<std::unique_ptr<ConsensusStrategy>> strategies,
                                  const std::string& defaultStrategy) {
    for (auto& strategy : strategies) {
        strategies_[strategy->name()] = std::move(strategy);
    }
    active_ = &require(defaultStrategy);
}

ConsensusStrategy& ConsensusEngine::require(const std::string& name) const {
    auto it = strategies_.find(toUpper(name));
    if (it == strategies_.end()) {
        std::string available;
        for (const auto& [key, value] : strategies_) {
            if (!available.empty()) available += ", ";
            available += key;
        }
        throw std::invalid_argument("Unknown consensus strategy: " + name + " (available: " + available + ")");
    }
    return *it->second;
}

std::vector<ConsensusStrategy*> ConsensusEngine::available() const {
    std::vector<ConsensusStrategy*> result;
    result.reserve(strategies_.size());
    for (const auto& [key, value] : strategies_) {
        result.push_back(value.get());
    }
    return result;
}

} // namespace legalchain::consensus
