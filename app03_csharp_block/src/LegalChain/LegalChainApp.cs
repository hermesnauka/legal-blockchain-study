using System.Text.Json.Serialization;
using LegalChain.Api;
using LegalChain.Config;
using LegalChain.Consensus;
using LegalChain.Contracts;
using LegalChain.Core;
using LegalChain.Crypto;
using LegalChain.Nft;
using LegalChain.P2p;

namespace LegalChain;

/// <summary>
/// Builds the fully wired LegalChain node as a <see cref="WebApplication"/>. Extracted
/// from <c>Program</c> so integration tests can boot two complete nodes (real Kestrel,
/// real WebSockets) inside one process — the C# equivalent of the Java node's two
/// Spring contexts in <c>TwoNodeSyncIntegrationTest</c>.
/// </summary>
/// <remarks>
/// All services are singletons registered in dependency order (the same object graph as
/// the Java node's Spring context and the C++ node's hand-wired <c>AppContext</c>):
/// EventBus → crypto services → NodeWallet → consensus → tokenomics → Blockchain →
/// contracts → NFT → P2P. ASP.NET Core's built-in container plays the role Spring plays
/// in app01 — an instructive contrast with app02's explicit constructor wiring.
/// </remarks>
public static class LegalChainApp
{
    public static WebApplication Build(string[] args)
    {
        var builder = WebApplication.CreateBuilder(args);

        // CLI overrides shared with the other nodes' run books: --port, --node-name.
        builder.Configuration.AddCommandLine(args, new Dictionary<string, string>
        {
            ["--port"] = "LegalChain:Port",
            ["--node-name"] = "LegalChain:NodeName"
        });

        var options = builder.Configuration.GetSection(LegalChainOptions.SectionName)
            .Get<LegalChainOptions>() ?? new LegalChainOptions();
        builder.Services.AddSingleton(options);

        builder.Services.AddSingleton<EventBus>();
        builder.Services.AddSingleton<PqcSignatureService>();
        builder.Services.AddSingleton<PqcKeyExchangeService>();
        builder.Services.AddSingleton<SecureChannel>();
        builder.Services.AddSingleton<NodeWallet>();
        builder.Services.AddSingleton<IConsensusStrategy, ProofOfWorkStrategy>();
        builder.Services.AddSingleton<IConsensusStrategy, ProofOfStakeStrategy>();
        builder.Services.AddSingleton<IConsensusStrategy, BftStrategy>();
        builder.Services.AddSingleton(sp => new ConsensusEngine(
            sp.GetServices<IConsensusStrategy>(), options.Consensus.DefaultStrategy));
        builder.Services.AddSingleton<TokenomicsService>();
        builder.Services.AddSingleton<Blockchain>();
        builder.Services.AddSingleton<MedicalConsentContract>();
        builder.Services.AddSingleton<AgriSupplyChainContract>();
        builder.Services.AddSingleton<ContractEngine>();
        builder.Services.AddSingleton<NftService>();
        builder.Services.AddSingleton<P2pService>();
        builder.Services.AddSingleton<P2pClientService>();
        builder.Services.AddSingleton<EventBroadcaster>();

        builder.Services.AddControllers().AddJsonOptions(o =>
        {
            // API contract v1: UPPERCASE enum names (TRANSFER, POS, ...) as strings.
            o.JsonSerializerOptions.Converters.Add(new JsonStringEnumConverter());
        });

        // Dev CORS for the Vite frontend (:5175); in dev the Vite proxy makes this moot,
        // but direct access (VITE_API_URL) must work too — parity with app01's CorsConfig.
        builder.Services.AddCors(o => o.AddDefaultPolicy(p =>
            p.AllowAnyOrigin().AllowAnyHeader().AllowAnyMethod()));

        // Bind the configured port unless the host explicitly overrode URLs
        // (tests pass --urls http://127.0.0.1:0 for ephemeral ports).
        if (args.All(a => !a.StartsWith("--urls", StringComparison.OrdinalIgnoreCase)))
        {
            builder.WebHost.UseUrls($"http://localhost:{options.Port}");
        }

        var app = builder.Build();

        app.UseCors();
        app.UseMiddleware<ApiExceptionMiddleware>();
        app.UseWebSockets();
        app.MapControllers();

        // Node-to-node link: PQC handshake + AES-256-GCM frames (see P2pService).
        app.Map("/ws/p2p", async context =>
        {
            if (!context.WebSockets.IsWebSocketRequest)
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                return;
            }
            var socket = await context.WebSockets.AcceptWebSocketAsync();
            var p2p = context.RequestServices.GetRequiredService<P2pService>();
            PeerSession peer = await p2p.RegisterAsync(socket, initiator: false, url: null);
            await p2p.ReceiveLoopAsync(peer, context.RequestAborted);
        });

        // Browser live feed: public ledger events, unencrypted by design.
        app.Map("/ws/events", async context =>
        {
            if (!context.WebSockets.IsWebSocketRequest)
            {
                context.Response.StatusCode = StatusCodes.Status400BadRequest;
                return;
            }
            var socket = await context.WebSockets.AcceptWebSocketAsync();
            var broadcaster = context.RequestServices.GetRequiredService<EventBroadcaster>();
            await broadcaster.AttachAsync(socket, context.RequestAborted);
        });

        // Eagerly create the event subscribers so no LedgerEvent is missed before the
        // first HTTP request touches them (DI singletons are otherwise lazy).
        app.Services.GetRequiredService<EventBroadcaster>();
        app.Services.GetRequiredService<P2pService>();

        return app;
    }
}
