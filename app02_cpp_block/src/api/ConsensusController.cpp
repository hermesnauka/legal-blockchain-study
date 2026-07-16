#include "api/ConsensusController.h"

#include <stdexcept>

#include "core/LedgerEvent.h"

using namespace drogon;

namespace legalchain::api {

void registerConsensusRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/consensus",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value response(Json::objectValue);
            response["active"] = ctx.consensus().active().name();
            Json::Value available(Json::arrayValue);
            for (auto* strategy : ctx.consensus().available()) {
                Json::Value entry(Json::objectValue);
                entry["name"] = strategy->name();
                entry["description"] = strategy->description();
                available.append(entry);
            }
            response["available"] = available;
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/consensus",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("strategy") || (*json)["strategy"].asString().empty()) {
                throw std::invalid_argument("strategy is required");
            }
            ctx.consensus().switchTo((*json)["strategy"].asString());
            Json::Value data(Json::objectValue);
            data["active"] = ctx.consensus().active().name();
            ctx.events().publish(core::LedgerEvent{core::LedgerEventType::CONSENSUS_CHANGED, data});
            callback(HttpResponse::newHttpJsonResponse(data));
        },
        {Post});
}

} // namespace legalchain::api
