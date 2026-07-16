#include "api/NftController.h"

#include <stdexcept>

using namespace drogon;

namespace legalchain::api {

void registerNftRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/nft/mint",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("title") || !json->isMember("metadataUri")) {
                throw std::invalid_argument("title and metadataUri are required");
            }
            std::string title = (*json)["title"].asString();
            std::string description = json->isMember("description") ? (*json)["description"].asString() : "";
            std::string metadataUri = (*json)["metadataUri"].asString();
            auto token = ctx.nftService().mint(title, description, metadataUri);
            callback(HttpResponse::newHttpJsonResponse(token.toJson()));
        },
        {Post});

    app.registerHandler(
        "/api/nft",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value response(Json::arrayValue);
            for (const auto& token : ctx.nftService().all()) response.append(token.toJson());
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});
}

} // namespace legalchain::api
