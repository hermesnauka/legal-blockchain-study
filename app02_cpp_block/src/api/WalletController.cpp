#include "api/WalletController.h"

using namespace drogon;

namespace legalchain::api {

void registerWalletRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/wallet",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto& wallet = ctx.wallet();
            auto balances = ctx.blockchain().balances();
            double balance = balances.count(wallet.address()) ? balances[wallet.address()] : 0.0;

            Json::Value response(Json::objectValue);
            response["address"] = wallet.address();
            response["algorithm"] = wallet.algorithm();
            response["publicKey"] = wallet.publicKeyBase64();
            response["fingerprint"] = wallet.address();
            response["balance"] = balance;
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/wallet/balances",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value response(Json::objectValue);
            for (const auto& [address, balance] : ctx.blockchain().balances()) {
                response[address] = balance;
            }
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});
}

} // namespace legalchain::api
