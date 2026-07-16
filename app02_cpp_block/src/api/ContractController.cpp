#include "api/ContractController.h"

#include <stdexcept>

#include "core/TransactionType.h"

using namespace drogon;

namespace legalchain::api {

namespace {
Json::Value consentRecordToJson(const contracts::MedicalConsentContract::ConsentRecord& r) {
    Json::Value v(Json::objectValue);
    v["patientId"] = r.patientId;
    v["granteeId"] = r.granteeId;
    v["scope"] = r.scope;
    v["granted"] = r.granted;
    v["timestamp"] = static_cast<Json::Int64>(r.timestampMs);
    return v;
}

Json::Value supplyChainEventToJson(const contracts::AgriSupplyChainContract::SupplyChainEvent& e) {
    Json::Value v(Json::objectValue);
    v["batchId"] = e.batchId;
    v["stage"] = e.stage;
    v["actor"] = e.actor;
    v["location"] = e.location;
    v["details"] = e.details;
    v["timestamp"] = static_cast<Json::Int64>(e.timestampMs);
    return v;
}
} // namespace

void registerContractRoutes(HttpAppFramework& app, config::AppContext& ctx) {
    app.registerHandler(
        "/api/contracts/medical/consent",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("patientId") || !json->isMember("granteeId")
                || !json->isMember("scope") || !json->isMember("granted")) {
                throw std::invalid_argument("patientId, granteeId, scope and granted are required");
            }
            std::map<std::string, std::string> input = {
                {"patientId", (*json)["patientId"].asString()},
                {"granteeId", (*json)["granteeId"].asString()},
                {"scope", (*json)["scope"].asString()},
                {"granted", (*json)["granted"].asBool() ? "true" : "false"}};
            auto result = ctx.contractEngine().execute(ctx.medical(), core::TransactionType::CONTRACT_MEDICAL, input);
            callback(HttpResponse::newHttpJsonResponse(result.toJson()));
        },
        {Post});

    app.registerHandler(
        "/api/contracts/medical/{patientId}",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback,
               const std::string& patientId) {
            Json::Value response(Json::arrayValue);
            for (const auto& record : ctx.medical().historyOf(patientId)) {
                response.append(consentRecordToJson(record));
            }
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});

    app.registerHandler(
        "/api/contracts/agri/event",
        [&ctx](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
            auto json = req->getJsonObject();
            if (!json || !json->isMember("batchId") || !json->isMember("stage") || !json->isMember("actor")
                || !json->isMember("location")) {
                throw std::invalid_argument("batchId, stage, actor and location are required");
            }
            std::map<std::string, std::string> input = {
                {"batchId", (*json)["batchId"].asString()},
                {"stage", (*json)["stage"].asString()},
                {"actor", (*json)["actor"].asString()},
                {"location", (*json)["location"].asString()},
                {"details", json->isMember("details") ? (*json)["details"].asString() : ""}};
            auto result = ctx.contractEngine().execute(ctx.agri(), core::TransactionType::CONTRACT_AGRI, input);
            callback(HttpResponse::newHttpJsonResponse(result.toJson()));
        },
        {Post});

    app.registerHandler(
        "/api/contracts/agri/{batchId}",
        [&ctx](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback,
               const std::string& batchId) {
            Json::Value response(Json::arrayValue);
            for (const auto& event : ctx.agri().trailOf(batchId)) {
                response.append(supplyChainEventToJson(event));
            }
            callback(HttpResponse::newHttpJsonResponse(response));
        },
        {Get});
}

} // namespace legalchain::api
