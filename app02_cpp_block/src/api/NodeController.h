#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// Node identity and P2P operations (connect to a peer, trigger a full ledger sync).
void registerNodeRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
