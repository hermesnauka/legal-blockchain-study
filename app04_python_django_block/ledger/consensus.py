"""
Consensus strategies (Strategy pattern): the ledger core is agnostic about *how* a
block earns the right to be appended. Each implementation seals a candidate (producing
its proof) and can independently re-validate any sealed block — validation must be a
pure function of the block, because remote nodes re-check proofs during P2P sync
without trusting the sender. This is the "Consensus and Scalability" seam of the
architecture: adding DPoS, Proof-of-Reputation, PoH or PoET later means adding one
class, not touching the core.
"""
from abc import ABC, abstractmethod

from . import crypto
from .core import Block, BlockCandidate


class ConsensusStrategy(ABC):
    """Seals candidates and re-validates sealed blocks; identified by a stable name."""

    @property
    @abstractmethod
    def name(self) -> str:
        """Stable identifier used in the API and stored in each block (consensusType)."""

    @property
    @abstractmethod
    def description(self) -> str:
        """One-line explanation surfaced by the educational frontend."""

    @abstractmethod
    def seal(self, candidate: BlockCandidate) -> Block:
        """Produces the proof for the candidate and returns the sealed, hashed block."""

    @abstractmethod
    def validate(self, block: Block) -> bool:
        """Re-verifies the proof of a sealed block (used when auditing peers' chains)."""


class ProofOfWorkStrategy(ConsensusStrategy):
    """
    Proof of Work: increment a nonce until the block hash carries a required prefix of
    zero hex digits. Security intuition: rewriting history requires redoing the
    accumulated work of every later block faster than the honest network extends it.
    The proof is trivially verifiable (one hash) but expensive to produce — an
    asymmetry that needs no identities at all. Trade-off: enormous energy cost, which
    is why this MVP defaults to PoS and keeps difficulty low (4 hex zeros ≈ 65k hashes).
    """

    DIFFICULTY_PREFIX = "0000"

    @property
    def name(self) -> str:
        return "POW"

    @property
    def description(self) -> str:
        return ("Proof of Work: nonce search until the SHA3-256 block hash starts with "
                f"{self.DIFFICULTY_PREFIX}; costly to produce, one hash to verify.")

    def seal(self, candidate: BlockCandidate) -> Block:
        nonce = 0
        while True:
            block = candidate.seal(self.name, f"difficulty={len(self.DIFFICULTY_PREFIX)}", nonce)
            if block.hash.startswith(self.DIFFICULTY_PREFIX):
                return block
            nonce += 1

    def validate(self, block: Block) -> bool:
        return block.hash.startswith(self.DIFFICULTY_PREFIX) and block.hash_is_consistent()


class ProofOfStakeStrategy(ConsensusStrategy):
    """
    Proof of Stake: block-sealing rights assigned by a stake-weighted lottery. The
    lottery must be deterministic and grinding-resistant so every node (including a
    peer receiving the block during sync) selects the same winner: it is seeded with
    the previous block hash — a value the sealer cannot freely re-roll — and walks the
    cumulative stake distribution. Security from "skin in the game", negligible energy
    use (ESG reporting is part of regulatory reality). MVP simplification: fixed,
    documented validator registry; production systems derive it from on-chain staking.
    """

    STAKES = (("validator-alpha", 60), ("validator-beta", 30), ("validator-gamma", 10))

    @property
    def name(self) -> str:
        return "POS"

    @property
    def description(self) -> str:
        return ("Proof of Stake: deterministic stake-weighted lottery seeded by the previous "
                "block hash; negligible energy use, security from economic stake.")

    def seal(self, candidate: BlockCandidate) -> Block:
        winner, stake = self._select_validator(candidate.previous_hash)
        proof = f"stake-winner={winner};stake={stake};seed={candidate.previous_hash[:12]}"
        return candidate.seal(self.name, proof, 0)

    def validate(self, block: Block) -> bool:
        # Recompute the lottery from the same seed: the proof must name the same winner.
        expected, _ = self._select_validator(block.previous_hash)
        return (block.proof.startswith(f"stake-winner={expected};")
                and block.hash_is_consistent())

    def _select_validator(self, previous_hash: str) -> tuple[str, int]:
        total = sum(stake for _, stake in self.STAKES)
        # First 12 hex chars of SHA3(previousHash) → uniform draw in [0, total).
        draw = int(crypto.sha3(previous_hash)[:12], 16) % total
        cumulative = 0
        for validator, stake in self.STAKES:
            cumulative += stake
            if draw < cumulative:
                return validator, stake
        raise RuntimeError("Stake distribution exhausted — unreachable")


class BftStrategy(ConsensusStrategy):
    """
    Byzantine Fault Tolerance (pBFT-style, simulated in-process panel). The classical
    result: with n = 3f + 1 validators, agreement survives f arbitrarily malicious
    ("Byzantine") members, because any two quorums of 2f + 1 votes intersect in at
    least one honest validator. Here n = 4, f = 1: a block seals only when ≥ 3 panel
    members approve the candidate's Merkle root. BFT gives immediate finality (no
    probabilistic confirmations, no forks) — precisely what permissioned, regulated
    ledgers need. Votes are simulated deterministically so peers can re-validate the
    recorded quorum; in production each vote would be an ML-DSA signature from a
    distinct node.
    """

    PANEL = ("validator-1", "validator-2", "validator-3", "validator-4")
    QUORUM = 3  # 2f + 1 with f = 1

    @property
    def name(self) -> str:
        return "BFT"

    @property
    def description(self) -> str:
        return ("Byzantine Fault Tolerance (pBFT): 4 validators, quorum of 3 (tolerates 1 "
                "malicious node); immediate finality, no forks.")

    def seal(self, candidate: BlockCandidate) -> Block:
        subject = candidate.merkle_root() + "|" + candidate.previous_hash
        votes = []
        for validator in self.PANEL:
            # Deterministic simulated vote: validator's attestation over the candidate.
            attestation = crypto.sha3(f"{validator}|{subject}")[:8]
            votes.append(f"{validator}:APPROVE:{attestation}")
        if len(votes) < self.QUORUM:
            raise RuntimeError("BFT quorum not reached")
        proof = f"quorum={len(votes)}/{len(self.PANEL)};votes=" + ",".join(votes)
        return candidate.seal(self.name, proof, 0)

    def validate(self, block: Block) -> bool:
        if not block.hash_is_consistent() or not block.proof.startswith("quorum="):
            return False
        # Re-verify each recorded attestation against the block's own commitments.
        subject = block.merkle_root + "|" + block.previous_hash
        votes_part = block.proof.split(";votes=", 1)[-1]
        valid = 0
        for vote in votes_part.split(","):
            parts = vote.split(":")
            if (len(parts) == 3 and parts[1] == "APPROVE"
                    and parts[2] == crypto.sha3(f"{parts[0]}|{subject}")[:8]):
                valid += 1
        return valid >= self.QUORUM


class ConsensusEngine:
    """
    Registry of strategies plus the currently active one; hot-swappable by name so the
    educational frontend can demonstrate different algorithms live.
    """

    def __init__(self, available: list[ConsensusStrategy], default_strategy: str):
        self._strategies = {s.name: s for s in available}
        self._active = self._require(default_strategy)

    def active(self) -> ConsensusStrategy:
        return self._active

    def by_name(self, name: str) -> ConsensusStrategy:
        return self._require(name)

    def switch_to(self, name: str) -> None:
        self._active = self._require(name)

    def available(self) -> list[ConsensusStrategy]:
        return sorted(self._strategies.values(), key=lambda s: s.name)

    def _require(self, name: str) -> ConsensusStrategy:
        strategy = self._strategies.get(name.upper())
        if strategy is None:
            raise ValueError(f"Unknown consensus strategy: {name} "
                             f"(available: {sorted(self._strategies)})")
        return strategy
