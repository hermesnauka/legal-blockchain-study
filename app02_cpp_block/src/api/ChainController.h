#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// The general ledger view: read the chain, audit it, and seal new blocks.
void registerChainRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
