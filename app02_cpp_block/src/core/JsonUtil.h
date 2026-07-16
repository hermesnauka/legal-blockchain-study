#pragma once

#include <json/json.h>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace legalchain::core::jsonutil {

/// Parses a compact JSON string; throws std::runtime_error on malformed input.
inline Json::Value parse(const std::string& text) {
    Json::CharReaderBuilder builder;
    Json::Value value;
    std::string errors;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(text.data(), text.data() + text.size(), &value, &errors)) {
        throw std::runtime_error("Malformed JSON: " + errors);
    }
    return value;
}

/// Serializes a Json::Value compactly (no pretty-printing), matching the wire
/// format expected by the frontend and by peer nodes.
inline std::string write(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

inline Json::Value mapToJson(const std::map<std::string, std::string>& m) {
    Json::Value v(Json::objectValue);
    for (const auto& [k, val] : m) {
        v[k] = val;
    }
    return v;
}

inline std::map<std::string, std::string> jsonToMap(const Json::Value& v) {
    std::map<std::string, std::string> m;
    if (v.isObject()) {
        for (const auto& key : v.getMemberNames()) {
            m[key] = v[key].asString();
        }
    }
    return m;
}

/// Deterministic, Java-Map-toString-flavoured rendering of a sorted string map,
/// used only as an internal building block of signed content — the exact byte
/// format only needs to be self-consistent between this node's signer and
/// verifier, not compatible with any other language's port.
inline std::string mapToCanonicalString(const std::map<std::string, std::string>& m) {
    std::string out = "{";
    bool first = true;
    for (const auto& [k, val] : m) {
        if (!first) out += ", ";
        first = false;
        out += k + "=" + val;
    }
    out += "}";
    return out;
}

} // namespace legalchain::core::jsonutil
