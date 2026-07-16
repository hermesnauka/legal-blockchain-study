#include "config/Config.h"

#include <fstream>
#include <sstream>

#include "core/JsonUtil.h"

namespace legalchain::config {

namespace {
std::string argValue(const std::string& arg, const std::string& flag) {
    // Supports both "--flag=value" and separate "--flag" "value" (handled by caller).
    std::string prefix = flag + "=";
    if (arg.rfind(prefix, 0) == 0) {
        return arg.substr(prefix.size());
    }
    return "";
}
} // namespace

Config Config::load(int argc, char** argv) {
    Config config;

    std::ifstream file("config/application.json");
    if (file) {
        std::ostringstream buffer;
        buffer << file.rdbuf();
        try {
            Json::Value json = core::jsonutil::parse(buffer.str());
            if (json.isMember("port")) config.port = json["port"].asInt();
            if (json.isMember("nodeName")) config.nodeName = json["nodeName"].asString();
            if (json.isMember("consensus") && json["consensus"].isMember("defaultStrategy")) {
                config.consensusDefault = json["consensus"]["defaultStrategy"].asString();
            }
            if (json.isMember("tokenomics")) {
                const auto& tok = json["tokenomics"];
                if (tok.isMember("blockReward")) config.blockReward = tok["blockReward"].asDouble();
                if (tok.isMember("halvingInterval")) config.halvingInterval = tok["halvingInterval"].asInt64();
                if (tok.isMember("maxSupply")) config.maxSupply = tok["maxSupply"].asDouble();
            }
        } catch (const std::exception&) {
            // Malformed config file: keep the built-in defaults rather than refusing to start.
        }
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (auto v = argValue(arg, "--port"); !v.empty()) {
            config.port = std::stoi(v);
        } else if (arg == "--port" && i + 1 < argc) {
            config.port = std::stoi(argv[++i]);
        } else if (auto v2 = argValue(arg, "--node-name"); !v2.empty()) {
            config.nodeName = v2;
        } else if (arg == "--node-name" && i + 1 < argc) {
            config.nodeName = argv[++i];
        }
    }

    return config;
}

} // namespace legalchain::config
