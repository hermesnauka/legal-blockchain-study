"""Consensus Strategy pattern: PoW, PoS, BFT sealing and independent re-validation."""
from dataclasses import replace

from django.test import SimpleTestCase

from ledger.consensus import BftStrategy, ConsensusEngine, ProofOfStakeStrategy, ProofOfWorkStrategy
from ledger.core import BlockCandidate


def _candidate() -> BlockCandidate:
    return BlockCandidate(1, 1700000000000, "0" * 64, (), "validator-test")


class ConsensusStrategyTest(SimpleTestCase):

    def test_proof_of_work_finds_difficulty_prefix_and_revalidates(self):
        strategy = ProofOfWorkStrategy()
        block = strategy.seal(_candidate())

        self.assertTrue(block.hash.startswith("0000"))
        self.assertTrue(strategy.validate(block))

        mutated = replace(block, proof=block.proof + "x")
        self.assertFalse(strategy.validate(mutated))

    def test_proof_of_stake_lottery_is_deterministic_per_seed(self):
        strategy = ProofOfStakeStrategy()
        candidate = _candidate()
        first = strategy.seal(candidate)
        second = strategy.seal(candidate)

        self.assertEqual(first.proof, second.proof)
        self.assertTrue(first.proof.startswith("stake-winner="))
        self.assertTrue(strategy.validate(first))

        forged = candidate.seal("POS", "stake-winner=validator-nobody;stake=99;seed=x", 0)
        self.assertFalse(strategy.validate(forged))

    def test_bft_records_a_verifiable_quorum_of_attestations(self):
        strategy = BftStrategy()
        candidate = _candidate()
        block = strategy.seal(candidate)

        self.assertTrue(block.proof.startswith("quorum=4/4"))
        self.assertTrue(strategy.validate(block))

        header, votes_part = block.proof.split(";votes=", 1)
        votes = votes_part.split(",")
        votes[0] = votes[0].rsplit(":", 1)[0] + ":deadbeef"
        one_bad_vote_proof = header + ";votes=" + ",".join(votes)
        one_bad_vote_block = candidate.seal("BFT", one_bad_vote_proof, 0)
        self.assertTrue(strategy.validate(one_bad_vote_block))  # 3/4 quorum survives

        below_quorum_proof = "quorum=2/4;votes=" + ",".join(votes_part.split(",")[:2])
        below_quorum_block = candidate.seal("BFT", below_quorum_proof, 0)
        self.assertFalse(strategy.validate(below_quorum_block))

    def test_engine_hot_swaps_by_name_and_rejects_unknown_strategies(self):
        engine = ConsensusEngine(
            [ProofOfWorkStrategy(), ProofOfStakeStrategy(), BftStrategy()], "POS")

        self.assertEqual(engine.active().name, "POS")
        engine.switch_to("bft")
        self.assertEqual(engine.active().name, "BFT")
        self.assertEqual([s.name for s in engine.available()], ["BFT", "POS", "POW"])

        with self.assertRaises(ValueError):
            engine.switch_to("POH")
        with self.assertRaises(ValueError):
            engine.by_name("DPOS")
