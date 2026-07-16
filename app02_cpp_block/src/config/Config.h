#pragma once

#include <cstdint>
#include <string>

namespace legalchain::config {

/// Node configuration: defaults from config/application.json, overridable on
/// the command line (--port, --node-name) so a second node instance can be
/// started for the two-node P2P demo without editing files.
struct Config {
    int port = 8090;
    std::string nodeName = "node-A";
    std::string consensusDefault = "POS";
    double blockReward = 50.0;
    int64_t halvingInterval = 100;
    double maxSupply = 21000.0;

    /// Loads config/application.json (relative to the executable) if present,
    /// then applies any recognized CLI flags on top.
    static Config load(int argc, char** argv);
};

} // namespace legalchain::config
