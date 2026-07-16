"""
Test-only helpers: a hand-wired node (no Django singleton) and an external test wallet
that signs transactions the way a browser-held key would, mirroring app03's TestNode.cs.
Not a test module itself (no `test_` prefix) so Django's test runner ignores it.
"""
import uuid
from dataclasses import replace
from decimal import Decimal

from ledger.consensus import BftStrategy, ConsensusEngine, ProofOfStakeStrategy, ProofOfWorkStrategy
from ledger.core import Blockchain, EventBus, Transaction, TransactionType, TokenomicsService, now_millis
from ledger.crypto import PqcSignatureService


class TestNode:
    """A blockchain wired directly from the domain layer, independent of ledger.context."""

    def __init__(self, default_strategy: str = "POS"):
        self.events = EventBus()
        self.signatures = PqcSignatureService()
        self.consensus = ConsensusEngine(
            [ProofOfWorkStrategy(), ProofOfStakeStrategy(), BftStrategy()], default_strategy)
        self.tokenomics = TokenomicsService(Decimal("50"), 100, Decimal("21000"))
        self.blockchain = Blockchain(self.consensus, self.tokenomics, self.signatures, self.events)

    def new_wallet(self) -> "TestWallet":
        return TestWallet(self.signatures)


class TestWallet:
    """An external, browser-like key holder that signs its own transactions."""

    def __init__(self, signatures: PqcSignatureService):
        self._signatures = signatures
        public_key, self._private_key = signatures.generate_keypair()
        self.public_key_b64 = signatures.encode_public_key(public_key)
        self.address = signatures.fingerprint(public_key)

    def signed_transfer(self, recipient: str, amount, memo: str = "") -> Transaction:
        unsigned = Transaction(
            id=str(uuid.uuid4()), timestamp=now_millis(),
            sender=self.address, recipient=recipient, amount=Decimal(str(amount)),
            type=TransactionType.TRANSFER, payload={"memo": memo} if memo else {},
            sender_public_key=self.public_key_b64, signature=None)
        return replace(unsigned, signature=self._signatures.sign(
            unsigned.content_to_sign(), self._private_key))
