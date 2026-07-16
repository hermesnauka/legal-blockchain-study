"""
API Contract v1 end-to-end audit: a single scripted ledger session driven entirely
through the Django test client, mirroring app01/app02/app03's ApiContractIntegrationTest.
One continuous test method by design: ledger.context.get_context() is a process-wide
singleton with no reset hook, so independent test methods would leak mutated chain/
mempool state between them in whatever order the runner picks.
"""
import json

from django.test import TestCase


class ApiContractIntegrationTest(TestCase):

    def _get(self, path):
        return self.client.get(path)

    def _post(self, path, body=None):
        return self.client.post(path, data=json.dumps(body or {}), content_type="application/json")

    def test_api_contract_v1_full_ledger_session(self):
        # 1. Node identity.
        node_info = self._get("/api/node").json()
        node_id = node_info["nodeId"]
        self.assertGreater(len(node_id), 16)
        self.assertIn("port", node_info)
        self.assertIn("consensus", node_info)
        self.assertEqual(node_info["peers"], [])
        self.assertEqual(node_info["chainLength"], 1)

        # 2. Genesis chain.
        chain_info = self._get("/api/chain").json()
        self.assertEqual(chain_info["length"], 1)
        self.assertTrue(chain_info["valid"])
        self.assertEqual(chain_info["blocks"][0]["index"], 0)
        self.assertRegex(chain_info["blocks"][0]["previousHash"], r"^0+$")

        # 3. Chain audit.
        validation = self._get("/api/chain/validate").json()
        self.assertTrue(validation["valid"])
        self.assertIn("passed", validation["message"])

        # 4. Wallet (SEC-08: no private-key material ever leaves the node).
        wallet_response = self._get("/api/wallet")
        wallet = wallet_response.json()
        self.assertIn("address", wallet)
        self.assertIn("fingerprint", wallet)
        self.assertIn("ML-DSA", wallet["algorithm"])
        self.assertNotIn("private", wallet_response.content.decode().lower())

        # 5. Overdraft is rejected with a 4xx error body; nothing enters the mempool.
        overdraft = self._post("/api/transactions", {"recipient": "mallory", "amount": 999999})
        self.assertEqual(overdraft.status_code, 400)
        self.assertIn("error", overdraft.json())
        self.assertEqual(self._get("/api/transactions/pending").json(), [])

        # 6. First mine: reward-only block.
        block1 = self._post("/api/chain/mine", {}).json()
        self.assertEqual(block1["index"], 1)
        self.assertEqual(block1["consensusType"], "POS")
        self.assertIn("merkleRoot", block1)
        reward_tx = block1["transactions"][0]
        self.assertEqual(reward_tx["sender"], "SYSTEM")
        self.assertEqual(reward_tx["recipient"], node_id)
        self.assertEqual(reward_tx["amount"], 50)

        # 7. Signed transfer enters the mempool.
        transfer = self._post("/api/transactions",
                              {"recipient": "alice", "amount": 2, "memo": "tuition"}).json()
        self.assertEqual(transfer["type"], "TRANSFER")
        self.assertIsNotNone(transfer["signature"])
        self.assertIsNotNone(transfer["senderPublicKey"])
        self.assertEqual(len(self._get("/api/transactions/pending").json()), 1)

        # 8. Second mine seals the transfer + reward.
        block2 = self._post("/api/chain/mine", {}).json()
        self.assertEqual(len(block2["transactions"]), 2)
        self.assertEqual(self._get("/api/transactions/pending").json(), [])

        # 9. Balances derived by replaying the chain.
        balances = self._get("/api/wallet/balances").json()
        self.assertEqual(balances["alice"], 2)
        self.assertEqual(balances[node_id], 98)

        # 10. Consensus hot-swap.
        consensus_state = self._get("/api/consensus").json()
        available_names = {option["name"] for option in consensus_state["available"]}
        self.assertTrue({"POW", "POS", "BFT"}.issubset(available_names))

        switched = self._post("/api/consensus", {"strategy": "BFT"}).json()
        self.assertEqual(switched["active"], "BFT")
        bft_block = self._post("/api/chain/mine", {}).json()
        self.assertEqual(bft_block["consensusType"], "BFT")
        self.assertIn("quorum", bft_block["proof"])
        self._post("/api/consensus", {"strategy": "POS"})

        # 11. NFT mint + gallery.
        nft = self._post("/api/nft/mint",
                         {"title": "Genesis Art", "description": "First mint",
                          "metadataUri": "ipfs://example"}).json()
        self.assertIn("tokenId", nft)
        self.assertIn("txId", nft)
        self.assertEqual(nft["creator"], node_id)
        self._post("/api/chain/mine", {})
        gallery = self._get("/api/nft").json()
        self.assertIn(nft["tokenId"], [item["tokenId"] for item in gallery])

        # 12. Medical consent contract: GDPR-safe grant/revoke audit trail.
        grant = self._post("/api/contracts/medical/consent",
                           {"patientId": "itest-patient", "granteeId": "dr-house",
                            "scope": "IMAGING", "granted": True}).json()
        self.assertTrue(grant["accepted"])
        self.assertEqual(set(grant["record"].keys()),
                         {"contract", "patientId", "granteeId", "scope", "granted"})
        self._post("/api/contracts/medical/consent",
                  {"patientId": "itest-patient", "granteeId": "dr-house",
                   "scope": "IMAGING", "granted": False})
        history = self._get("/api/contracts/medical/itest-patient").json()
        self.assertEqual(len(history), 2)
        self.assertTrue(history[0]["granted"])
        self.assertFalse(history[1]["granted"])

        bad_scope = self._post("/api/contracts/medical/consent",
                               {"patientId": "itest-patient", "granteeId": "dr-house",
                                "scope": "TELEPATHY", "granted": True})
        self.assertEqual(bad_scope.status_code, 400)
        self._post("/api/chain/mine", {})

        # 13. Agri supply-chain contract: forward-only stage trail.
        self._post("/api/contracts/agri/event",
                   {"batchId": "IT-BATCH-1", "stage": "HARVEST",
                    "actor": "farmer", "location": "field-1", "details": ""})
        self._post("/api/contracts/agri/event",
                   {"batchId": "IT-BATCH-1", "stage": "TRANSPORT",
                    "actor": "carrier", "location": "road", "details": ""})
        trail = self._get("/api/contracts/agri/IT-BATCH-1").json()
        self.assertEqual([e["stage"] for e in trail], ["HARVEST", "TRANSPORT"])

        regression = self._post("/api/contracts/agri/event",
                                {"batchId": "IT-BATCH-1", "stage": "PROCESSING",
                                 "actor": "farmer", "location": "field-1", "details": ""})
        self.assertEqual(regression.status_code, 400)
        self._post("/api/chain/mine", {})

        # 14. i18n dictionaries: identical key sets across languages, unsupported → 400.
        en = self._get("/api/i18n/en").json()
        pl = self._get("/api/i18n/pl").json()
        self.assertTrue(en)
        self.assertEqual(set(en.keys()), set(pl.keys()))
        self.assertNotEqual(en["app.title"], pl["app.title"])
        self.assertEqual(self._get("/api/i18n/de").status_code, 400)

        # 15. Final audit: the whole scripted session is still a valid ledger.
        self.assertTrue(self._get("/api/chain/validate").json()["valid"])
