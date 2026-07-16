#include "api/TransactionController.h"

#include <stdexcept>

#include "core/IdGen.h"

using namespace drogon;

namespace legalchain::api {

void registerTransactionRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/transactions/pending",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback) {
            Json::Value response(Json::arrayValue);
            for (const auto& tx : ctx.blockchain().pendingTransactions()) {
                response.append(tx.toJson());
            }
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/transactions",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("recipient") || !json->isMember("amount")) {
                throw std::invalid_argument("recipient and amount are required");
            }
            std::string recipient = (*json)["recipient"].asString();
            double amount = (*json)["amount"].asDouble();
            if (recipient.empty()) throw std::invalid_argument("recipient must not be blank");
            if (amount < 0) throw std::invalid_argument("amount must be >= 0");
            std::map<std::string, std::string> payload;
            if (json->isMember("memo") && !(*json)["memo"].asString().empty()) {
                payload["memo"] = (*json)["memo"].asString();
            }

            auto& wallet = ctx.wallet();
            core::Transaction unsigned_(core::newUuid(), core::nowMillis(), wallet.address(), recipient, amount,
                                         core::TransactionType::TRANSFER, payload, wallet.publicKeyBase64(),
                                         std::nullopt);
            core::Transaction signed_(unsigned_.id(), unsigned_.timestampMs(), unsigned_.sender(),
                                       unsigned_.recipient(), unsigned_.amount(), unsigned_.type(),
                                       unsigned_.payload(), unsigned_.senderPublicKey(),
                                       wallet.sign(unsigned_.contentToSign()));

            core::Transaction result = ctx.blockchain().submit(signed_);
            callback(HttpResponse::newHttpJsonResponse(result.toJson()));
        },
        {Post});
}

} // namespace legalchain::api
