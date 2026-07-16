"""Ledger core: signature/overdraft rejection, mining, tamper-evidence, chain adoption."""
from dataclasses import replace
from decimal import Decimal

from django.test import SimpleTestCase

from ledger.core import TokenomicsService
from ledger.tests.support import TestNode


class BlockchainTest(SimpleTestCase):

    def test_rejects_unsigned_and_forged_transactions(self):
        node = TestNode()
        wallet = node.new_wallet()

        unsigned = wallet.signed_transfer("alice", 10)
        unsigned = replace(unsigned, signature=None, sender_public_key=None)
        with self.assertRaises(ValueError):
            node.blockchain.submit(unsigned)

        forged = replace(wallet.signed_transfer("alice", 10), sender="lc-someone-else")
        with self.assertRaises(ValueError):
            node.blockchain.submit(forged)

        self.assertEqual(node.blockchain.pending_transactions(), [])

    def test_rejects_overdraft_including_pending_outgoing(self):
        node = TestNode()
        wallet = node.new_wallet()
        node.blockchain.mine_pending_transactions(wallet.address)  # +50 LGC

        with self.assertRaises(ValueError):
            node.blockchain.submit(wallet.signed_transfer("alice", 51))

        node.blockchain.submit(wallet.signed_transfer("alice", 30))
        with self.assertRaises(ValueError):
            node.blockchain.submit(wallet.signed_transfer("bob", 30))

        self.assertEqual(len(node.blockchain.pending_transactions()), 1)

    def test_mining_seals_transfers_with_reward_and_balances_replay_from_genesis(self):
        node = TestNode()
        wallet = node.new_wallet()
        node.blockchain.mine_pending_transactions(wallet.address)
        node.blockchain.submit(wallet.signed_transfer("alice", "12.5", memo="coffee"))
        block = node.blockchain.mine_pending_transactions("validator-x")

        self.assertEqual(len(block.transactions), 2)
        self.assertEqual(node.blockchain.pending_transactions(), [])
        self.assertTrue(block.hash_is_consistent())
        self.assertEqual(block.consensus_type, "POS")

        balances = node.blockchain.balances()
        self.assertEqual(balances["alice"], Decimal("12.5"))
        self.assertEqual(balances[wallet.address], Decimal("37.5"))
        self.assertEqual(balances["validator-x"], Decimal("50"))
        self.assertTrue(node.blockchain.is_valid())

    def test_tampering_with_any_historical_transaction_invalidates_the_chain(self):
        node = TestNode()
        wallet = node.new_wallet()
        node.blockchain.mine_pending_transactions(wallet.address)
        node.blockchain.submit(wallet.signed_transfer("alice", 5))
        node.blockchain.mine_pending_transactions(wallet.address)

        blocks = node.blockchain.blocks()
        victim = blocks[2]
        mutated_tx = replace(victim.transactions[0], amount=Decimal(500))
        tampered_block = replace(victim, transactions=(mutated_tx,) + victim.transactions[1:])
        tampered_chain = blocks[:2] + [tampered_block] + blocks[3:]

        self.assertFalse(node.blockchain.is_valid_chain(tampered_chain))
        self.assertTrue(node.blockchain.is_valid())

    def test_replace_chain_adopts_only_strictly_longer_fully_valid_histories(self):
        node_a = TestNode()
        node_b = TestNode()
        node_a.blockchain.mine_pending_transactions("validator-a")
        node_a.blockchain.mine_pending_transactions("validator-a")
        node_b.blockchain.mine_pending_transactions("validator-b")

        self.assertFalse(node_a.blockchain.replace_chain(node_b.blockchain.blocks()))
        self.assertTrue(node_b.blockchain.replace_chain(node_a.blockchain.blocks()))
        self.assertEqual(node_b.blockchain.length(), 3)
        self.assertTrue(node_b.blockchain.is_valid())

        node_a.blockchain.mine_pending_transactions("validator-a")
        blocks = node_a.blockchain.blocks()
        forged_block = replace(blocks[1], validator_id="mallory")
        forged_chain = [blocks[0], forged_block] + blocks[2:]
        self.assertFalse(node_b.blockchain.replace_chain(forged_chain))

    def test_reward_halves_on_schedule_under_the_supply_cap(self):
        tokenomics = TokenomicsService(Decimal("50"), 100, Decimal("21000"))
        self.assertEqual(tokenomics.reward_for_block(1, Decimal(0)), Decimal(50))
        self.assertEqual(tokenomics.reward_for_block(99, Decimal(0)), Decimal(50))
        self.assertEqual(tokenomics.reward_for_block(100, Decimal(0)), Decimal(25))
        self.assertEqual(tokenomics.reward_for_block(200, Decimal(0)), Decimal("12.5"))
        self.assertEqual(tokenomics.reward_for_block(1, Decimal(20997)), Decimal(3))
        self.assertEqual(tokenomics.reward_for_block(1, Decimal(21000)), Decimal(0))
