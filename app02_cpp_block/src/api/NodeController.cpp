#include "api/NodeController.h"

#include <stdexcept>

using namespace drogon;

namespace legalchain::api {

void registerNodeRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/node",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value response(Json::objectValue);
            response["nodeId"] = ctx.wallet().address();
            response["name"] = ctx.config().nodeName;
            response["port"] = ctx.config().port;
            response["consensus"] = ctx.consensus().active().name();
            response["peers"] = ctx.p2pService().connectedPeers();
            response["chainLength"] = ctx.blockchain().length();
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/p2p/connect",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("url") || (*json)["url"].asString().empty()) {
                throw std::invalid_argument("url is required");
            }
            std::string url = (*json)["url"].asString();
            ctx.p2pClient().connect(url);
            Json::Value response(Json::objectValue);
            response["connected"] = true;
            response["peer"] = url;
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Post});

    app.registerHandler(
        "/api/p2p/sync",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            int asked = ctx.p2pService().syncWithPeers();
            Json::Value response(Json::objectValue);
            response["result"] = asked > 0 ? ("Sync requested from " + std::to_string(asked) + " peer(s)")
                                            : "No established peers to sync with";
            response["chainLength"] = ctx.blockchain().length();
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Post});
}

} // namespace legalchain::api
