#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// Sector smart contracts: medical consent registry and agri supply-chain traceability.
void registerContractRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
