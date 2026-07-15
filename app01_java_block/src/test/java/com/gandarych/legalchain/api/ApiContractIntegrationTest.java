package com.gandarych.legalchain.api;

import org.junit.jupiter.api.MethodOrderer;
import org.junit.jupiter.api.Order;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestMethodOrder;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.core.ParameterizedTypeReference;
import org.springframework.http.HttpEntity;
import org.springframework.http.HttpMethod;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

import java.util.List;
import java.util.Map;
import java.util.Set;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

/**
 * Contract-level integration test for API Contract v1 (checklist BE-15): every REST
 * endpoint is exercised against a real running node — full Spring context, real
 * ML-DSA signing, real consensus — and each response is checked against the shape
 * and rules the frontend relies on. Ordered as one auditable ledger session:
 * identity → genesis → mempool rules → mining → transfers → consensus hot-swap →
 * NFT → smart contracts → i18n → final full-chain audit.
 */
@SpringBootTest(
        webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT,
        properties = "legalchain.node.name=api-itest")
@TestMethodOrder(MethodOrderer.OrderAnnotation.class)
class ApiContractIntegrationTest {

    private static final ParameterizedTypeReference<Map<String, Object>> MAP =
            new ParameterizedTypeReference<>() {
            };
    private static final ParameterizedTypeReference<List<Map<String, Object>>> LIST =
            new ParameterizedTypeReference<>() {
            };

    @Autowired
    private TestRestTemplate rest;

    private Map<String, Object> get(String path) {
        ResponseEntity<Map<String, Object>> r = rest.exchange(path, HttpMethod.GET, null, MAP);
        assertTrue(r.getStatusCode().is2xxSuccessful(), path + " should be 2xx, was " + r.getStatusCode());
        return r.getBody();
    }

    private List<Map<String, Object>> getList(String path) {
        ResponseEntity<List<Map<String, Object>>> r = rest.exchange(path, HttpMethod.GET, null, LIST);
        assertTrue(r.getStatusCode().is2xxSuccessful(), path + " should be 2xx, was " + r.getStatusCode());
        return r.getBody();
    }

    private ResponseEntity<Map<String, Object>> post(String path, Map<String, ?> body) {
        return rest.exchange(path, HttpMethod.POST, new HttpEntity<>(body), MAP);
    }

    private Map<String, Object> postOk(String path, Map<String, ?> body) {
        ResponseEntity<Map<String, Object>> r = post(path, body);
        assertTrue(r.getStatusCode().is2xxSuccessful(), path + " should be 2xx, was " + r.getStatusCode());
        return r.getBody();
    }

    private String nodeId() {
        return (String) get("/api/node").get("nodeId");
    }

    @SuppressWarnings("unchecked")
    private static List<Map<String, Object>> transactionsOf(Map<String, Object> block) {
        return (List<Map<String, Object>>) block.get("transactions");
    }

    @Test
    @Order(1)
    void nodeEndpointExposesFullIdentityContract() {
        Map<String, Object> node = get("/api/node");
        assertTrue(((String) node.get("nodeId")).length() > 16, "nodeId is a key fingerprint");
        assertEquals("api-itest", node.get("name"));
        assertNotNull(node.get("port"));
        assertNotNull(node.get("consensus"));
        assertTrue(node.get("peers") instanceof List, "peers is a list");
        assertEquals(1, ((Number) node.get("chainLength")).intValue(), "fresh node starts at genesis");
    }

    @Test
    @Order(2)
    void chainStartsWithValidGenesisAndPassesAudit() {
        Map<String, Object> chain = get("/api/chain");
        assertEquals(1, ((Number) chain.get("length")).intValue());
        assertEquals(Boolean.TRUE, chain.get("valid"));
        @SuppressWarnings("unchecked")
        Map<String, Object> genesis = ((List<Map<String, Object>>) chain.get("blocks")).getFirst();
        assertEquals(0, ((Number) genesis.get("index")).intValue());
        assertTrue(((String) genesis.get("previousHash")).matches("0+"), "genesis links to the zero hash");

        Map<String, Object> audit = get("/api/chain/validate");
        assertEquals(Boolean.TRUE, audit.get("valid"));
        assertTrue(((String) audit.get("message")).contains("passed"));
    }

    @Test
    @Order(3)
    void walletExposesPseudonymousIdentityAndNeverKeyMaterial() {
        Map<String, Object> wallet = get("/api/wallet");
        assertNotNull(wallet.get("address"));
        assertNotNull(wallet.get("fingerprint"));
        assertTrue(((String) wallet.get("algorithm")).contains("ML-DSA"), "PQC signature algorithm advertised");

        // SEC-08: no private key material over any API — check the raw JSON text.
        String raw = rest.getForObject("/api/wallet", String.class);
        assertFalse(raw.toLowerCase().contains("private"), "wallet response must not carry private key material");
    }

    @Test
    @Order(4)
    void overdraftIsRejectedWith4xxAndNotQueued() {
        ResponseEntity<Map<String, Object>> r = post("/api/transactions",
                Map.of("recipient", "mallory", "amount", 999_999));
        assertEquals(HttpStatus.BAD_REQUEST, r.getStatusCode(), "BE-03: overdraft → 4xx");
        assertNotNull(r.getBody().get("error"), "uniform error body");
        assertTrue(getList("/api/transactions/pending").isEmpty(), "rejected tx must not reach the mempool");
    }

    @Test
    @Order(5)
    void miningSealsRewardTransactionViaActiveConsensus() {
        Map<String, Object> block = postOk("/api/chain/mine", Map.of());
        assertEquals(1, ((Number) block.get("index")).intValue());
        assertEquals("POS", block.get("consensusType"), "default strategy from configuration");
        assertNotNull(block.get("merkleRoot"));

        List<Map<String, Object>> txs = transactionsOf(block);
        assertEquals(1, txs.size());
        Map<String, Object> reward = txs.getFirst();
        assertEquals("REWARD", reward.get("type"));
        assertEquals("SYSTEM", reward.get("sender"));
        assertEquals(nodeId(), reward.get("recipient"));
        assertEquals(50, ((Number) reward.get("amount")).intValue(), "initial tokenomics reward");
    }

    @Test
    @Order(6)
    void signedTransferFlowsThroughMempoolIntoBalances() {
        Map<String, Object> tx = postOk("/api/transactions",
                Map.of("recipient", "alice", "amount", 2, "memo", "tuition"));
        assertEquals("TRANSFER", tx.get("type"));
        assertNotNull(tx.get("signature"), "every transfer is ML-DSA signed");
        assertNotNull(tx.get("senderPublicKey"), "public key travels with the tx for offline audit");

        assertEquals(1, getList("/api/transactions/pending").size());
        Map<String, Object> block = postOk("/api/chain/mine", Map.of());
        assertEquals(2, transactionsOf(block).size(), "transfer + validator reward");
        assertTrue(getList("/api/transactions/pending").isEmpty(), "mempool drained after sealing");

        Map<String, Object> balances = get("/api/wallet/balances");
        assertEquals(2, ((Number) balances.get("alice")).intValue());
        assertEquals(98, ((Number) balances.get(nodeId())).intValue(), "2×50 reward − 2 sent");
    }

    @Test
    @Order(7)
    void consensusStrategyHotSwapsAndShapesTheNextBlock() {
        Map<String, Object> state = get("/api/consensus");
        @SuppressWarnings("unchecked")
        List<Map<String, Object>> available = (List<Map<String, Object>>) state.get("available");
        Set<Object> names = Set.copyOf(available.stream().map(a -> a.get("name")).toList());
        assertTrue(names.containsAll(Set.of("POW", "POS", "BFT")), "executable strategies advertised");

        assertEquals("BFT", postOk("/api/consensus", Map.of("strategy", "BFT")).get("active"));
        Map<String, Object> block = postOk("/api/chain/mine", Map.of());
        assertEquals("BFT", block.get("consensusType"));
        assertTrue(((String) block.get("proof")).contains("quorum"), "pBFT proof records the vote set");

        assertEquals("POS", postOk("/api/consensus", Map.of("strategy", "POS")).get("active"));
    }

    @Test
    @Order(8)
    void nftMintBindsTokenToCreatorFingerprint() {
        Map<String, Object> nft = postOk("/api/nft/mint", Map.of(
                "title", "Integration Artwork",
                "description", "minted by the API contract test",
                "metadataUri", "ipfs://QmIntegration"));
        assertNotNull(nft.get("tokenId"));
        assertNotNull(nft.get("txId"), "mint is itself a ledger transaction");
        assertEquals(nodeId(), nft.get("creator"), "SSI: creator is the signing key's fingerprint");

        postOk("/api/chain/mine", Map.of());
        assertTrue(getList("/api/nft").stream().anyMatch(n -> nft.get("tokenId").equals(n.get("tokenId"))),
                "gallery lists the minted token");
    }

    @Test
    @Order(9)
    void medicalConsentLifecycleIsAuditableAndDataMinimal() {
        Map<String, Object> grant = postOk("/api/contracts/medical/consent", Map.of(
                "patientId", "itest-patient", "granteeId", "dr-house",
                "scope", "LAB_RESULTS", "granted", true));
        assertEquals(Boolean.TRUE, grant.get("accepted"));
        @SuppressWarnings("unchecked")
        Map<String, Object> record = (Map<String, Object>) grant.get("record");
        assertEquals(Set.of("contract", "patientId", "granteeId", "scope", "granted"), record.keySet(),
                "BE-11: only pseudonymous ids/scope/decision on-chain — never clinical data");

        postOk("/api/contracts/medical/consent", Map.of(
                "patientId", "itest-patient", "granteeId", "dr-house",
                "scope", "LAB_RESULTS", "granted", false));
        List<Map<String, Object>> history = getList("/api/contracts/medical/itest-patient");
        assertEquals(2, history.size(), "revocation appends — it never rewrites the grant");
        assertEquals(Boolean.TRUE, history.getFirst().get("granted"));
        assertEquals(Boolean.FALSE, history.getLast().get("granted"));

        ResponseEntity<Map<String, Object>> bad = post("/api/contracts/medical/consent", Map.of(
                "patientId", "itest-patient", "granteeId", "dr-house",
                "scope", "TELEPATHY", "granted", true));
        assertEquals(HttpStatus.BAD_REQUEST, bad.getStatusCode(), "closed scope vocabulary");
        postOk("/api/chain/mine", Map.of());
    }

    @Test
    @Order(10)
    void agriTrailIsChronologicalAndForwardOnly() {
        postOk("/api/contracts/agri/event", Map.of(
                "batchId", "IT-BATCH-1", "stage", "HARVEST",
                "actor", "Green Farm", "location", "Lublin", "details", "organic"));
        postOk("/api/contracts/agri/event", Map.of(
                "batchId", "IT-BATCH-1", "stage", "TRANSPORT",
                "actor", "ColdTrans", "location", "Warszawa", "details", ""));

        List<Map<String, Object>> trail = getList("/api/contracts/agri/IT-BATCH-1");
        assertEquals(List.of("HARVEST", "TRANSPORT"), trail.stream().map(e -> e.get("stage")).toList(),
                "BE-12: chronological farm-to-fork trail");

        ResponseEntity<Map<String, Object>> regression = post("/api/contracts/agri/event", Map.of(
                "batchId", "IT-BATCH-1", "stage", "PROCESSING",
                "actor", "Shady Ltd", "location", "nowhere", "details", "backdated"));
        assertEquals(HttpStatus.BAD_REQUEST, regression.getStatusCode(),
                "stage regression (the document-fraud case) is rejected");
        postOk("/api/chain/mine", Map.of());
    }

    @Test
    @Order(11)
    void i18nDictionariesAreServedCompleteAndParallel() {
        Map<String, Object> en = get("/api/i18n/en");
        Map<String, Object> pl = get("/api/i18n/pl");
        assertFalse(en.isEmpty());
        assertEquals(en.keySet(), pl.keySet(), "I18N-03: identical key sets in both languages");
        assertNotEquals(en.get("app.title"), pl.get("app.title"), "values are actually translated");
    }

    @Test
    @Order(12)
    void fullChainAuditStillPassesAfterAllActivity() {
        Map<String, Object> audit = get("/api/chain/validate");
        assertEquals(Boolean.TRUE, audit.get("valid"),
                "hashes, links, signatures and proofs all verify after the whole session");
    }
}
