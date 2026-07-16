#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "consensus/ConsensusStrategy.h"

namespace legalchain::consensus {

/// Holds the registry of ConsensusStrategy instances and the currently active
/// one. Adding a new consensus algorithm becomes available API-wide by
/// constructing it and passing it into this engine — no other code changes.
class ConsensusEngine {
public:
    explicit ConsensusEngine(std::vector<std::unique_ptr<ConsensusStrategy>> strategies,
                              const std::string& defaultStrategy = "POS");

    ConsensusStrategy& active() const { return *active_; }
    ConsensusStrategy& byName(const std::string& name) const { return require(name); }
    void switchTo(const std::string& name) { active_ = &require(name); }

    /// Strategies sorted by name.
    std::vector<ConsensusStrategy*> available() const;

private:
    std::map<std::string, std::unique_ptr<ConsensusStrategy>> strategies_;
    ConsensusStrategy* active_ = nullptr;

    ConsensusStrategy& require(const std::string& name) const;
    static std::string toUpper(const std::string& s);
};

} // namespace legalchain::consensus
