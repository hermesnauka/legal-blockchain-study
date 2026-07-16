using System.Net;
using System.Net.Http.Json;
using System.Text.Json;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.AspNetCore.Hosting;

namespace LegalChain.Tests;

/// <summary>
/// Contract-level integration test for API Contract v1 (checklist BE-15): every REST
/// endpoint is exercised against a real node — full DI graph, real ML-DSA signing, real
/// consensus — and each response is checked against the shape and rules the frontend
/// relies on. Runs as one auditable ledger session: identity → genesis → mempool rules →
/// mining → transfers → consensus hot-swap → NFT → smart contracts → i18n → final audit.
/// </summary>
public class ApiContractIntegrationTest : IClassFixture<ApiContractIntegrationTest.NodeFactory>
{
    public class NodeFactory : WebApplicationFactory<Program>
    {
        protected override void ConfigureWebHost(IWebHostBuilder builder)
        {
            builder.UseSetting("LegalChain:NodeName", "api-itest");
        }
    }

    private readonly HttpClient _client;

    public ApiContractIntegrationTest(NodeFactory factory)
    {
        _client = factory.CreateClient();
    }

    private async Task<JsonElement> GetJson(string path)
    {
        HttpResponseMessage response = await _client.GetAsync(path);
        Assert.True(response.IsSuccessStatusCode, path + " should be 2xx, was " + response.StatusCode);
        return await response.Content.ReadFromJsonAsync<JsonElement>();
    }

    private async Task<HttpResponseMessage> Post(string path, object body)
        => await _client.PostAsJsonAsync(path, body);

    private async Task<JsonElement> PostOk(string path, object body)
    {
        HttpResponseMessage response = await Post(path, body);
        Assert.True(response.IsSuccessStatusCode, path + " should be 2xx, was " + response.StatusCode);
        return await response.Content.ReadFromJsonAsync<JsonElement>();
    }

    private async Task<string> NodeId()
        => (await GetJson("/api/node")).GetProperty("nodeId").GetString()!;

    [Fact]
    public async Task ApiContractV1FullLedgerSession()
    {
        // --- /api/node: full identity contract -------------------------------------
        JsonElement node = await GetJson("/api/node");
        Assert.True(node.GetProperty("nodeId").GetString()!.Length > 16, "nodeId is a key fingerprint");
        Assert.Equal("api-itest", node.GetProperty("name").GetString());
        Assert.True(node.TryGetProperty("port", out _));
        Assert.True(node.TryGetProperty("consensus", out _));
        Assert.Equal(JsonValueKind.Array, node.GetProperty("peers").ValueKind);
        Assert.Equal(1, node.GetProperty("chainLength").GetInt32());

        // --- genesis + audit --------------------------------------------------------
        JsonElement chain = await GetJson("/api/chain");
        Assert.Equal(1, chain.GetProperty("length").GetInt32());
        Assert.True(chain.GetProperty("valid").GetBoolean());
        JsonElement genesis = chain.GetProperty("blocks")[0];
        Assert.Equal(0, genesis.GetProperty("index").GetInt64());
        Assert.Matches("^0+$", genesis.GetProperty("previousHash").GetString());

        JsonElement audit = await GetJson("/api/chain/validate");
        Assert.True(audit.GetProperty("valid").GetBoolean());
        Assert.Contains("passed", audit.GetProperty("message").GetString());

        // --- wallet: pseudonymous identity, never key material (SEC-08) --------------
        JsonElement wallet = await GetJson("/api/wallet");
        Assert.True(wallet.TryGetProperty("address", out _));
        Assert.True(wallet.TryGetProperty("fingerprint", out _));
        Assert.Contains("ML-DSA", wallet.GetProperty("algorithm").GetString());
        string rawWallet = await _client.GetStringAsync("/api/wallet");
        Assert.DoesNotContain("private", rawWallet.ToLowerInvariant());

        // --- BE-03: overdraft → 4xx with uniform error body, not queued --------------
        HttpResponseMessage overdraft = await Post("/api/transactions",
            new { recipient = "mallory", amount = 999_999 });
        Assert.Equal(HttpStatusCode.BadRequest, overdraft.StatusCode);
        JsonElement error = await overdraft.Content.ReadFromJsonAsync<JsonElement>();
        Assert.True(error.TryGetProperty("error", out _), "uniform error body");
        Assert.Equal(0, (await GetJson("/api/transactions/pending")).GetArrayLength());

        // --- mining seals the tokenomics reward via the active strategy --------------
        JsonElement block1 = await PostOk("/api/chain/mine", new { });
        Assert.Equal(1, block1.GetProperty("index").GetInt64());
        Assert.Equal("POS", block1.GetProperty("consensusType").GetString());
        Assert.True(block1.TryGetProperty("merkleRoot", out _));
        JsonElement reward = block1.GetProperty("transactions")[0];
        Assert.Equal("REWARD", reward.GetProperty("type").GetString());
        Assert.Equal("SYSTEM", reward.GetProperty("sender").GetString());
        Assert.Equal(await NodeId(), reward.GetProperty("recipient").GetString());
        Assert.Equal(50m, reward.GetProperty("amount").GetDecimal());

        // --- signed transfer flows through mempool into balances ---------------------
        JsonElement tx = await PostOk("/api/transactions",
            new { recipient = "alice", amount = 2, memo = "tuition" });
        Assert.Equal("TRANSFER", tx.GetProperty("type").GetString());
        Assert.True(tx.TryGetProperty("signature", out var sig) && sig.ValueKind == JsonValueKind.String);
        Assert.True(tx.TryGetProperty("senderPublicKey", out _));

        Assert.Equal(1, (await GetJson("/api/transactions/pending")).GetArrayLength());
        JsonElement block2 = await PostOk("/api/chain/mine", new { });
        Assert.Equal(2, block2.GetProperty("transactions").GetArrayLength());
        Assert.Equal(0, (await GetJson("/api/transactions/pending")).GetArrayLength());

        JsonElement balances = await GetJson("/api/wallet/balances");
        Assert.Equal(2m, balances.GetProperty("alice").GetDecimal());
        Assert.Equal(98m, balances.GetProperty(await NodeId()).GetDecimal()); // 2×50 − 2

        // --- consensus hot-swap shapes the next block (BE-08/BE-09) ------------------
        JsonElement consensus = await GetJson("/api/consensus");
        var names = consensus.GetProperty("available").EnumerateArray()
            .Select(s => s.GetProperty("name").GetString()).ToHashSet();
        Assert.Superset(new HashSet<string?> { "POW", "POS", "BFT" }, names);

        Assert.Equal("BFT",
            (await PostOk("/api/consensus", new { strategy = "BFT" })).GetProperty("active").GetString());
        JsonElement bftBlock = await PostOk("/api/chain/mine", new { });
        Assert.Equal("BFT", bftBlock.GetProperty("consensusType").GetString());
        Assert.Contains("quorum", bftBlock.GetProperty("proof").GetString());
        await PostOk("/api/consensus", new { strategy = "POS" });

        // --- NFT mint binds token to creator fingerprint (BE-13) ---------------------
        JsonElement nft = await PostOk("/api/nft/mint", new
        {
            title = "Integration Artwork",
            description = "minted by the API contract test",
            metadataUri = "ipfs://QmIntegration"
        });
        string tokenId = nft.GetProperty("tokenId").GetString()!;
        Assert.True(nft.TryGetProperty("txId", out _), "mint is itself a ledger transaction");
        Assert.Equal(await NodeId(), nft.GetProperty("creator").GetString());
        await PostOk("/api/chain/mine", new { });
        JsonElement gallery = await GetJson("/api/nft");
        Assert.Contains(gallery.EnumerateArray(),
            n => n.GetProperty("tokenId").GetString() == tokenId);

        // --- medical consent lifecycle (BE-11) ---------------------------------------
        JsonElement grant = await PostOk("/api/contracts/medical/consent", new
        {
            patientId = "itest-patient",
            granteeId = "dr-house",
            scope = "LAB_RESULTS",
            granted = true
        });
        Assert.True(grant.GetProperty("accepted").GetBoolean());
        var recordKeys = grant.GetProperty("record").EnumerateObject().Select(p => p.Name).ToHashSet();
        Assert.Equal(new HashSet<string> { "contract", "patientId", "granteeId", "scope", "granted" },
            recordKeys);

        await PostOk("/api/contracts/medical/consent", new
        {
            patientId = "itest-patient",
            granteeId = "dr-house",
            scope = "LAB_RESULTS",
            granted = false
        });
        JsonElement history = await GetJson("/api/contracts/medical/itest-patient");
        Assert.Equal(2, history.GetArrayLength());
        Assert.True(history[0].GetProperty("granted").GetBoolean());
        Assert.False(history[1].GetProperty("granted").GetBoolean());

        HttpResponseMessage badScope = await Post("/api/contracts/medical/consent", new
        {
            patientId = "itest-patient",
            granteeId = "dr-house",
            scope = "TELEPATHY",
            granted = true
        });
        Assert.Equal(HttpStatusCode.BadRequest, badScope.StatusCode);
        await PostOk("/api/chain/mine", new { });

        // --- agri trail: chronological and forward-only (BE-12) ----------------------
        await PostOk("/api/contracts/agri/event", new
        {
            batchId = "IT-BATCH-1",
            stage = "HARVEST",
            actor = "Green Farm",
            location = "Lublin",
            details = "organic"
        });
        await PostOk("/api/contracts/agri/event", new
        {
            batchId = "IT-BATCH-1",
            stage = "TRANSPORT",
            actor = "ColdTrans",
            location = "Warszawa",
            details = ""
        });
        JsonElement trail = await GetJson("/api/contracts/agri/IT-BATCH-1");
        Assert.Equal(["HARVEST", "TRANSPORT"],
            trail.EnumerateArray().Select(e => e.GetProperty("stage").GetString()).ToList());

        HttpResponseMessage regression = await Post("/api/contracts/agri/event", new
        {
            batchId = "IT-BATCH-1",
            stage = "PROCESSING",
            actor = "Shady Ltd",
            location = "nowhere",
            details = "backdated"
        });
        Assert.Equal(HttpStatusCode.BadRequest, regression.StatusCode);
        await PostOk("/api/chain/mine", new { });

        // --- I18N-03/04: complete, parallel dictionaries -----------------------------
        JsonElement en = await GetJson("/api/i18n/en");
        JsonElement pl = await GetJson("/api/i18n/pl");
        var enKeys = en.EnumerateObject().Select(p => p.Name).ToHashSet();
        var plKeys = pl.EnumerateObject().Select(p => p.Name).ToHashSet();
        Assert.NotEmpty(enKeys);
        Assert.Equal(enKeys, plKeys);
        Assert.NotEqual(en.GetProperty("app.title").GetString(), pl.GetProperty("app.title").GetString());
        Assert.Equal(HttpStatusCode.BadRequest, (await _client.GetAsync("/api/i18n/de")).StatusCode);

        // --- final full-chain audit after the whole session --------------------------
        JsonElement finalAudit = await GetJson("/api/chain/validate");
        Assert.True(finalAudit.GetProperty("valid").GetBoolean());
    }
}
