#include "api/I18nController.h"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "core/JsonUtil.h"

using namespace drogon;

namespace legalchain::api {

namespace {
std::mutex cacheMutex;
std::unordered_map<std::string, Json::Value> cache;

Json::Value load(const std::string& lang) {
    std::ifstream file("i18n/messages_" + lang + ".json");
    if (!file) {
        throw std::runtime_error("Missing i18n bundle for " + lang);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return core::jsonutil::parse(buffer.str());
}
} // namespace

void registerI18nRoutes(HttpAppFramework& app) {
    app.registerHandler(
        "/api/i18n/{lang}",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback,
           const std::string& lang) {
            std::string normalized = lang;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
            if (normalized != "en" && normalized != "pl") {
                throw std::invalid_argument("Unsupported language: " + lang + " (supported: en, pl)");
            }
            std::lock_guard<std::mutex> lock(cacheMutex);
            auto it = cache.find(normalized);
            if (it == cache.end()) {
                it = cache.emplace(normalized, load(normalized)).first;
            }
            callback(HttpResponse::newHttpJsonResponse(it->second));
        },
        {Get});
}

} // namespace legalchain::api
