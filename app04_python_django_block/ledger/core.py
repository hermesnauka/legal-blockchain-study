"""
The ledger core: immutable transactions and blocks, deterministic genesis, tokenomics,
the node wallet and the Blockchain itself.

This is a real ledger, not a mock: balances are never stored — they are derived by
replaying every transaction from genesis; signatures are verified with ML-DSA before a
transaction is accepted; and `is_valid_chain` re-audits hashes, links, Merkle roots,
signatures and consensus proofs of the entire history. That
auditability-from-first-principles is what makes a blockchain acceptable as a compliant
book of record.
"""
import threading
import time
import uuid
from dataclasses import dataclass, field, replace
from decimal import Decimal
from enum import Enum

from . import crypto
from .crypto import PqcSignatureService


def now_millis() -> int:
    return int(time.time() * 1000)


def format_amount(amount: Decimal) -> str:
    """Canonical plain decimal rendering without trailing zeros (BigDecimal-style)."""
    return format(amount.normalize(), "f")


def json_number(amount: Decimal):
    """Serialize Decimal as a JSON number: int when integral, float otherwise."""
    return int(amount) if amount == amount.to_integral_value() else float(amount)


class TransactionType(str, Enum):
    """Ledger entry categories; contract types make sector use-cases auditable on-chain."""
    TRANSFER = "TRANSFER"
    REWARD = "REWARD"
    NFT_MINT = "NFT_MINT"
    CONTRACT_MEDICAL = "CONTRACT_MEDICAL"
    CONTRACT_AGRI = "CONTRACT_AGRI"


SYSTEM = "SYSTEM"


@dataclass(frozen=True)
class Transaction:
    """
    Immutable ledger transaction (frozen dataclass — immutability is a compliance
    feature: once created, an entry cannot be mutated in memory, only superseded
    on-chain). Every non-reward transaction is signed with the sender's ML-DSA key over
    `content_to_sign()`, and carries the sender's public key so any node or external
    auditor can verify authorship offline. The sender "address" is a SHA3-256
    fingerprint of that key — pseudonymous, self-sovereign identity.
    """
    id: str
    timestamp: int
    sender: str
    recipient: str
    amount: Decimal
    type: TransactionType
    payload: dict | None
    sender_public_key: str | None
    signature: str | None

    def content_to_sign(self) -> str:
        """
        Canonical string covered by the ML-DSA signature. The payload map is serialized
        in sorted-key order so signer and verifier always derive the same bytes.
        """
        items = sorted((self.payload or {}).items())
        payload_str = "{" + ", ".join(f"{k}={v}" for k, v in items) + "}"
        return (f"{self.id}|{self.timestamp}|{self.sender}|{self.recipient}|"
                f"{format_amount(self.amount)}|{self.type.value}|{payload_str}")

    def is_reward(self) -> bool:
        return self.type == TransactionType.REWARD

    def to_dict(self) -> dict:
        return {
            "id": self.id,
            "timestamp": self.timestamp,
            "sender": self.sender,
            "recipient": self.recipient,
            "amount": json_number(self.amount),
            "type": self.type.value,
            "payload": self.payload or {},
            "senderPublicKey": self.sender_public_key,
            "signature": self.signature,
        }

    @staticmethod
    def from_dict(d: dict) -> "Transaction":
        return Transaction(
            id=d["id"],
            timestamp=int(d["timestamp"]),
            sender=d["sender"],
            recipient=d["recipient"],
            amount=Decimal(str(d["amount"])),
            type=TransactionType(d["type"]),
            payload=dict(d.get("payload") or {}),
            sender_public_key=d.get("senderPublicKey"),
            signature=d.get("signature"),
        )


@dataclass(frozen=True)
class Block:
    """
    Immutable block. The header hash covers index, timestamp, previous hash, Merkle
    root, validator, consensus proof and nonce: because previousHash of block N+1 is
    the hash of block N, retroactively editing any historical transaction invalidates
    every later block — the core tamper-evidence property of a compliant registry.
    """
    index: int
    timestamp: int
    previous_hash: str
    merkle_root: str
    transactions: tuple
    validator_id: str
    consensus_type: str
    proof: str
    nonce: int
    hash: str

    @staticmethod
    def compute_hash(index: int, timestamp: int, previous_hash: str, merkle_root: str,
                     validator_id: str, consensus_type: str, proof: str, nonce: int) -> str:
        return crypto.sha3(f"{index}|{timestamp}|{previous_hash}|{merkle_root}|"
                           f"{validator_id}|{consensus_type}|{proof}|{nonce}")

    @staticmethod
    def merkle_root_of(transactions) -> str:
        return crypto.merkle_root([tx.content_to_sign() for tx in transactions])

    def hash_is_consistent(self) -> bool:
        """True if the stored hash matches a recomputation of the header — tamper check."""
        return self.hash == Block.compute_hash(
            self.index, self.timestamp, self.previous_hash,
            Block.merkle_root_of(self.transactions),
            self.validator_id, self.consensus_type, self.proof, self.nonce)

    def to_dict(self) -> dict:
        return {
            "index": self.index,
            "timestamp": self.timestamp,
            "previousHash": self.previous_hash,
            "merkleRoot": self.merkle_root,
            "transactions": [tx.to_dict() for tx in self.transactions],
            "validatorId": self.validator_id,
            "consensusType": self.consensus_type,
            "proof": self.proof,
            "nonce": self.nonce,
            "hash": self.hash,
        }

    @staticmethod
    def from_dict(d: dict) -> "Block":
        return Block(
            index=int(d["index"]),
            timestamp=int(d["timestamp"]),
            previous_hash=d["previousHash"],
            merkle_root=d["merkleRoot"],
            transactions=tuple(Transaction.from_dict(t) for t in d.get("transactions", [])),
            validator_id=d["validatorId"],
            consensus_type=d["consensusType"],
            proof=d["proof"],
            nonce=int(d["nonce"]),
            hash=d["hash"],
        )


@dataclass(frozen=True)
class BlockCandidate:
    """
    An unsealed block handed to a consensus strategy, which produces the proof/nonce
    and the final Block. Separating the candidate from the sealed block is what lets
    consensus algorithms be swapped (Strategy pattern) without touching the ledger core.
    """
    index: int
    timestamp: int
    previous_hash: str
    transactions: tuple
    validator_id: str

    def merkle_root(self) -> str:
        return Block.merkle_root_of(self.transactions)

    def seal(self, consensus_type: str, proof: str, nonce: int) -> Block:
        root = self.merkle_root()
        block_hash = Block.compute_hash(self.index, self.timestamp, self.previous_hash,
                                        root, self.validator_id, consensus_type, proof, nonce)
        return Block(self.index, self.timestamp, self.previous_hash, root,
                     self.transactions, self.validator_id, consensus_type, proof,
                     nonce, block_hash)


#: Deterministic genesis: identical on every node, so two fresh remote nodes share a common root.
GENESIS = Block(
    index=0, timestamp=0, previous_hash="0" * 64,
    merkle_root=Block.merkle_root_of(()), transactions=(),
    validator_id="GENESIS", consensus_type="GENESIS", proof="genesis", nonce=0,
    hash=Block.compute_hash(0, 0, "0" * 64, Block.merkle_root_of(()),
                            "GENESIS", "GENESIS", "genesis", 0))


class LedgerEventType(str, Enum):
    """Wire names for /ws/events — UPPER_SNAKE, identical across all four apps."""
    BLOCK_ADDED = "BLOCK_ADDED"
    TX_ADDED = "TX_ADDED"
    PEER_CONNECTED = "PEER_CONNECTED"
    CHAIN_REPLACED = "CHAIN_REPLACED"
    CONSENSUS_CHANGED = "CONSENSUS_CHANGED"


@dataclass(frozen=True)
class LedgerEvent:
    """Domain event relayed to the educational frontend at /ws/events."""
    type: LedgerEventType
    data: dict


class EventBus:
    """
    Minimal in-process publish/subscribe bus — the Python analogue of Spring's
    ApplicationEventPublisher used by the Java node. A failing observer (e.g. a dead
    WebSocket) must never break the ledger write that raised the event.
    """

    def __init__(self):
        self._subscribers: list = []
        self._lock = threading.Lock()

    def subscribe(self, subscriber) -> None:
        with self._lock:
            self._subscribers.append(subscriber)

    def publish(self, event: LedgerEvent) -> None:
        with self._lock:
            snapshot = list(self._subscribers)
        for subscriber in snapshot:
            try:
                subscriber(event)
            except Exception:
                pass


class TokenomicsService:
    """
    Educational tokenomics: a fixed block reward that halves every N blocks under a
    hard supply cap. Compliance angle: total supply and issuance schedule are
    deterministic functions of the chain itself — no discretionary "printing"; an
    auditor can recompute the entire monetary base from the ledger (the transparency
    property regulators, e.g. under MiCA, expect from a token issuer).
    """

    def __init__(self, block_reward: Decimal, halving_interval: int, max_supply: Decimal):
        self.block_reward = block_reward
        self.halving_interval = halving_interval
        self.max_supply = max_supply

    def reward_for_block(self, block_index: int, supply_already_issued: Decimal) -> Decimal:
        halvings = block_index // self.halving_interval
        reward = self.block_reward / (Decimal(2) ** halvings)
        remaining = self.max_supply - supply_already_issued
        return max(min(reward, remaining), Decimal(0))

    def reward_transaction(self, validator_address: str, block_index: int,
                           supply_already_issued: Decimal) -> Transaction:
        """Coinbase-style reward transaction crediting the sealing validator."""
        return Transaction(
            id=str(uuid.uuid4()), timestamp=now_millis(),
            sender=SYSTEM, recipient=validator_address,
            amount=self.reward_for_block(block_index, supply_already_issued),
            type=TransactionType.REWARD,
            payload={"reason": "block-reward", "block": str(block_index)},
            sender_public_key=None, signature=None)

    def issued_supply(self, chain: list[Block]) -> Decimal:
        """Total supply issued so far = sum of all REWARD transactions in the chain."""
        return sum((tx.amount for b in chain for tx in b.transactions if tx.is_reward()),
                   Decimal(0))


class NodeWallet:
    """
    The node's post-quantum wallet: an ML-DSA-65 key pair generated at startup.
    Self-sovereign identity in practice — the address is the SHA3-256 fingerprint of
    the public key; no registration authority issues it, and it reveals nothing about
    the operator, yet every signature is independently verifiable. The private key
    never leaves this process (MVP: not persisted; restart = new identity).
    """

    def __init__(self, signatures: PqcSignatureService):
        self._signatures = signatures
        public_key, self._private_key = signatures.generate_keypair()
        self.public_key_b64 = signatures.encode_public_key(public_key)
        self.address = signatures.fingerprint(public_key)

    def sign(self, content: str) -> str:
        return self._signatures.sign(content, self._private_key)

    def algorithm(self) -> str:
        return self._signatures.algorithm_name()


class Blockchain:
    """
    The general ledger: an append-only, hash-chained list of blocks plus the pool of
    pending (signed, not yet sealed) transactions. Thread-safety: mutating operations
    hold the chain lock; reads return copies; events are published outside the lock.
    """

    def __init__(self, consensus_engine, tokenomics: TokenomicsService,
                 signatures: PqcSignatureService, events: EventBus):
        self._chain: list[Block] = [GENESIS]
        self._pending: list[Transaction] = []
        self._lock = threading.Lock()
        self._consensus = consensus_engine
        self._tokenomics = tokenomics
        self._signatures = signatures
        self._events = events

    def submit(self, tx: Transaction) -> Transaction:
        """
        Accepts a transaction into the pending pool after verifying its ML-DSA
        signature, that the claimed sender address really is the fingerprint of the
        signing key (otherwise anyone could sign as somebody else with their own key),
        and that the sender can cover the amount: confirmed balance minus what they
        already have pending. Rewards are exempt — the only sanctioned money creation,
        bounded by TokenomicsService. Counting pending outgoing amounts closes the
        mempool double-spend, so no sealed block can ever drive a balance negative —
        the first invariant an auditor checks in any book of record.
        """
        if not tx.is_reward():
            if not tx.sender_public_key or not tx.signature:
                raise ValueError("Transaction must be signed (ML-DSA)")
            if not self._signatures.verify(tx.content_to_sign(), tx.signature,
                                           tx.sender_public_key):
                raise ValueError("Invalid ML-DSA signature")
            expected = self._signatures.fingerprint(
                self._signatures.decode_public_key(tx.sender_public_key))
            if expected != tx.sender:
                raise ValueError("Sender address does not match signing key")
        with self._lock:
            if not tx.is_reward():
                available = self._balances_locked().get(tx.sender, Decimal(0)) \
                    - self._pending_outgoing_of(tx.sender)
                if tx.amount > available:
                    raise ValueError(
                        f"Insufficient funds: sender {tx.sender} has "
                        f"{format_amount(available)} LGC available but tried to send "
                        f"{format_amount(tx.amount)} LGC")
            self._pending.append(tx)
        self._events.publish(LedgerEvent(LedgerEventType.TX_ADDED, tx.to_dict()))
        return tx

    def _pending_outgoing_of(self, sender: str) -> Decimal:
        return sum((tx.amount for tx in self._pending
                    if not tx.is_reward() and tx.sender == sender), Decimal(0))

    def mine_pending_transactions(self, validator_address: str) -> Block:
        """
        Seals the pending transactions into a new block via the active consensus
        strategy and credits the validator with the tokenomics reward.
        """
        with self._lock:
            previous = self._chain[-1]
            index = previous.index + 1
            block_txs = tuple(self._pending) + (self._tokenomics.reward_transaction(
                validator_address, index, self._tokenomics.issued_supply(self._chain)),)
            candidate = BlockCandidate(index, now_millis(), previous.hash,
                                       block_txs, validator_address)
            block = self._consensus.active().seal(candidate)
            self._chain.append(block)
            self._pending.clear()
        self._events.publish(LedgerEvent(LedgerEventType.BLOCK_ADDED, block.to_dict()))
        return block

    def is_valid_chain(self, candidate: list[Block]) -> bool:
        """
        Full audit of a chain: genesis identity, hash consistency, previous-hash links,
        transaction signatures, and each block's own consensus proof (looked up by the
        strategy recorded in the block, so mixed-consensus histories stay verifiable).
        """
        if not candidate or candidate[0] != GENESIS:
            return False
        for i in range(1, len(candidate)):
            block, previous = candidate[i], candidate[i - 1]
            if (block.index != previous.index + 1
                    or block.previous_hash != previous.hash
                    or not block.hash_is_consistent()):
                return False
            try:
                if not self._consensus.by_name(block.consensus_type).validate(block):
                    return False
            except ValueError:
                return False
            for tx in block.transactions:
                if not tx.is_reward() and not self._signatures.verify(
                        tx.content_to_sign(), tx.signature, tx.sender_public_key):
                    return False
        return True

    def is_valid(self) -> bool:
        return self.is_valid_chain(self.blocks())

    def replace_chain(self, candidate: list[Block]) -> bool:
        """
        Longest-valid-chain rule used by P2P sync: adopt a peer's chain only if it is
        strictly longer AND passes the full local audit — a malicious peer cannot push
        a forged history, because we never trust, we always re-verify.
        """
        with self._lock:
            if len(candidate) <= len(self._chain) or not self.is_valid_chain(candidate):
                return False
            self._chain = list(candidate)
            self._pending.clear()
            length = len(candidate)
        self._events.publish(LedgerEvent(LedgerEventType.CHAIN_REPLACED, {"length": length}))
        return True

    def balances(self) -> dict[str, Decimal]:
        """Balances derived by replaying the full ledger — the "general ledger" property."""
        with self._lock:
            return self._balances_locked()

    def _balances_locked(self) -> dict[str, Decimal]:
        balances: dict[str, Decimal] = {}
        for block in self._chain:
            for tx in block.transactions:
                if not tx.is_reward():
                    balances[tx.sender] = balances.get(tx.sender, Decimal(0)) - tx.amount
                balances[tx.recipient] = balances.get(tx.recipient, Decimal(0)) + tx.amount
        return balances

    def blocks(self) -> list[Block]:
        with self._lock:
            return list(self._chain)

    def pending_transactions(self) -> list[Transaction]:
        with self._lock:
            return list(self._pending)

    def length(self) -> int:
        with self._lock:
            return len(self._chain)
