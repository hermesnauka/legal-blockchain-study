#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// NFT minting and gallery.
void registerNftRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
