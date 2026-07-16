#include <drogon/drogon.h>

#include "api/ApiExceptionHandler.h"
#include "api/ChainController.h"
#include "api/ConsensusController.h"
#include "api/ContractController.h"
#include "api/I18nController.h"
#include "api/NftController.h"
#include "api/NodeController.h"
#include "api/TransactionController.h"
#include "api/WalletController.h"
#include "config/AppContext.h"
#include "config/Config.h"
#include "p2p/EventBroadcaster.h"
#include "p2p/P2pWebSocketController.h"

using namespace drogon;
using namespace legalchain;

int main(int argc, char** argv) {
    config::Config config = config::Config::load(argc, argv);
    // Lives for the duration of app.run(); every route handler below captures
    // it by reference, which stays valid because this stack frame outlives
    // the whole event loop (app.run() only returns on shutdown).
    config::AppContext ctx(config);

    auto& app = drogon::app();

    // CORS: the dev SPA (Vite on :5174) and the production build both need
    // cross-origin access to this API during local development.
    app.registerPreRoutingAdvice(
        [](const HttpRequestPtr& req, AdviceCallback&& acb, AdviceChainCallback&& accb) {
            if (req->method() == HttpMethod::Options) {
                auto resp = HttpResponse::newHttpResponse();
                resp->addHeader("Access-Control-Allow-Origin", "*");
                resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers", "Content-Type");
                acb(resp);
                return;
            }
            accb();
        });
    app.registerPostHandlingAdvice(
        [](const HttpRequestPtr&, const HttpResponsePtr& resp) { resp->addHeader("Access-Control-Allow-Origin", "*"); });

    api::registerExceptionHandler(app);
    api::registerChainRoutes(app, ctx);
    api::registerTransactionRoutes(app, ctx);
    api::registerNodeRoutes(app, ctx);
    api::registerWalletRoutes(app, ctx);
    api::registerNftRoutes(app, ctx);
    api::registerContractRoutes(app, ctx);
    api::registerConsensusRoutes(app, ctx);
    api::registerI18nRoutes(app);

    app.registerController(std::make_shared<p2p::P2pWebSocketController>(ctx.p2pService()));
    app.registerController(std::make_shared<p2p::EventBroadcaster>(ctx.events()));

    LOG_INFO << "LegalChain node '" << config.nodeName << "' starting on port " << config.port
             << " (wallet " << ctx.wallet().address() << ", consensus " << ctx.consensus().active().name() << ")";

    app.addListener("0.0.0.0", config.port);
    app.setThreadNum(4);
    app.run();
    return 0;
}
