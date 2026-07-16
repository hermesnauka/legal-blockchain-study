#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// This node's wallet identity and the derived ledger balances.
void registerWalletRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
