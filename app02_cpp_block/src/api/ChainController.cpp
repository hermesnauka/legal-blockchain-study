#include "api/ChainController.h"

#include "core/JsonUtil.h"

using namespace drogon;

namespace legalchain::api {

void registerChainRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/chain",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto blocks = ctx.blockchain().blocks();
            Json::Value response(Json::objectValue);
            response["length"] = static_cast<Json::UInt64>(blocks.size());
            response["valid"] = ctx.blockchain().isValid();
            Json::Value blocksJson(Json::arrayValue);
            for (const auto& block : blocks) blocksJson.append(block.toJson());
            response["blocks"] = blocksJson;
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/chain/validate",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            bool valid = ctx.blockchain().isValid();
            Json::Value response(Json::objectValue);
            response["valid"] = valid;
            response["message"] = valid
                ? "Chain audit passed: all hashes, links, ML-DSA signatures and consensus proofs verified."
                : "Chain audit FAILED: at least one block or signature does not verify.";
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/chain/mine",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            std::string validator = ctx.wallet().address();
            auto json = req->getJsonObject();
            if (json && json->isMember("validatorId") && !(*json)["validatorId"].asString().empty()) {
                validator = (*json)["validatorId"].asString();
            }
            core::Block block = ctx.blockchain().minePendingTransactions(validator);
            callback(HttpResponse::newHttpJsonResponse(block.toJson()));
        },
        {Post});
}

} // namespace legalchain::api
