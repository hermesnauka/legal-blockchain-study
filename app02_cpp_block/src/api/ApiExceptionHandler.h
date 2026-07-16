#pragma once

#include <drogon/HttpAppFramework.h>

namespace legalchain::api {

/// Central exception -> HTTP response mapping, the C++ analogue of the Java
/// port's @ControllerAdvice ApiExceptionHandler: std::invalid_argument
/// (validation failures, signature/funds/contract-rule violations) become
/// 400 Bad Request with a JSON {"error": message} body; anything else
/// becomes 500 so a bug never leaks a raw stack trace to the frontend.
void registerExceptionHandler(drogon::HttpAppFramework& app);

} // namespace legalchain::api
