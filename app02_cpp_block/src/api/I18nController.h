#pragma once

#include <drogon/HttpAppFramework.h>

namespace legalchain::api {

/// Server-side dictionaries backing the mandatory PL/EN language switcher:
/// the frontend keeps its own UI dictionaries, and this endpoint serves the
/// shared, backend-owned strings (consensus/PQC descriptions) so both nodes
/// present identical wording.
void registerI18nRoutes(drogon::HttpAppFramework& app);

} // namespace legalchain::api
