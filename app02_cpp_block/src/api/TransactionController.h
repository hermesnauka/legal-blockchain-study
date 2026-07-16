#pragma once

#include <drogon/HttpAppFramework.h>

#include "config/AppContext.h"

namespace legalchain::api {

/// Token transfers. The browser has no key store in the MVP, so the node
/// wallet signs on the user's behalf (custodial model); the ML-DSA signature
/// is created here and verified again by Blockchain::submit — the ledger
/// never trusts the API layer.
void registerTransactionRoutes(drogon::HttpAppFramework& app, config::AppContext& ctx);

} // namespace legalchain::api
