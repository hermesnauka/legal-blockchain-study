#include "api/ApiExceptionHandler.h"

#include <stdexcept>

namespace legalchain::api {

namespace {
drogon::HttpResponsePtr errorResponse(drogon::HttpStatusCode status, const std::string& message) {
    Json::Value body(Json::objectValue);
    body["error"] = message;
    auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(status);
    return resp;
}
} // namespace

void registerExceptionHandler(drogon::HttpAppFramework& app) {
    app.setExceptionHandler([](const std::exception& e, const drogon::HttpRequestPtr&,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        if (dynamic_cast<const std::invalid_argument*>(&e) != nullptr) {
            callback(errorResponse(drogon::k400BadRequest, e.what()));
        } else {
            callback(errorResponse(drogon::k500InternalServerError, e.what()));
        }
    });
}

} // namespace legalchain::api
