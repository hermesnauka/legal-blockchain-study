"""
Two independent nodes converge on one ledger over the PQC-secured P2P protocol.

ledger.context.get_context() is a process-wide singleton, so a real two-node test must
bypass it and wire two independently constructed AppContexts together directly — using
get_context() for both "sides" would make node-A and node-B literally the same node.
"""
from django.test import SimpleTestCase

from ledger.context import AppContext


class TwoNodeSyncTest(SimpleTestCase):

    def test_two_remote_nodes_establish_pqc_channel_and_converge_on_one_ledger(self):
        node_a = AppContext(node_name="sync-itest-A")
        node_b = AppContext(node_name="sync-itest-B")

        # BE-14: distinct ML-DSA identities.
        self.assertNotEqual(node_a.wallet.address, node_b.wallet.address)
        # BE-02: identical deterministic genesis on every fresh node.
        self.assertEqual(node_a.blockchain.blocks()[0].hash, node_b.blockchain.blocks()[0].hash)

        node_a.blockchain.mine_pending_transactions(node_a.wallet.address)
        node_a.blockchain.mine_pending_transactions(node_a.wallet.address)
        target_length = node_a.blockchain.length()

        # Wire the two nodes' P2pService instances directly to each other: each side's
        # `send` callable feeds straight into the other side's on_message, with no real
        # socket in between. A mutable `peers` dict (rather than plain locals) resolves
        # the mutual-recursion ordering — both sessions must exist as named objects
        # before either side's callback can reference the other.
        peers = {}

        def send_to_b(text):
            node_b.p2p.on_message(peers["b"], text)

        def send_to_a(text):
            node_a.p2p.on_message(peers["a"], text)

        peers["a"] = node_a.p2p.register(send_to_b, initiator=False)
        peers["b"] = node_b.p2p.register(send_to_a, initiator=False, url="in-process://node-a")

        # Drive the ML-DSA/ML-KEM handshake explicitly (HELLO -> ENCAPS -> CHAIN_REQUEST/
        # CHAIN_RESPONSE all run synchronously inside this one call): neither register()
        # call above triggered it automatically (initiator=False on both), because doing
        # so here — rather than via the initiator=True auto-send — avoids calling back
        # into a peer session before its own register() call has returned it.
        node_b.p2p._send_hello(peers["b"])

        self.assertTrue(peers["a"].established)
        self.assertTrue(peers["b"].established)

        # The CHAIN_REQUEST/CHAIN_RESPONSE exchange above already adopted node A's
        # longer chain; sync_with_peers() re-asks explicitly, exercising BE's P2P sync
        # entry point directly.
        node_b.p2p.sync_with_peers()

        self.assertGreaterEqual(node_b.blockchain.length(), target_length)
        self.assertEqual(node_a.blockchain.blocks()[-1].hash, node_b.blockchain.blocks()[-1].hash)
        self.assertTrue(node_b.blockchain.is_valid())

    def test_a_forged_peer_chain_is_rejected_by_the_local_audit(self):
        node_a = AppContext(node_name="forge-itest-A")
        node_b = AppContext(node_name="forge-itest-B")
        node_a.blockchain.mine_pending_transactions(node_a.wallet.address)

        from dataclasses import replace
        blocks = node_a.blockchain.blocks()
        forged = [blocks[0], replace(blocks[1], validator_id="mallory")]

        self.assertFalse(node_b.blockchain.replace_chain(forged))
        self.assertEqual(node_b.blockchain.length(), 1)
