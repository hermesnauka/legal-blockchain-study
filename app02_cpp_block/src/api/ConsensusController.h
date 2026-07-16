#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// Active consensus strategy + the encyclopedia list of available ones.
void registerConsensusRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
