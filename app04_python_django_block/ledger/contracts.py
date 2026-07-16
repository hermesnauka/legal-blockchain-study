"""
Blockchain 3.0 smart contracts: deterministic business logic whose every execution is
recorded as a signed ledger transaction.

Design rule for compliance (GDPR art. 17 "right to erasure" vs. immutability):
contracts keep personal data OFF-chain and record on-chain only hashes, pseudonymous
identifiers and consent/state transitions. The chain then proves *that* and *when*
something happened without ever containing erasable personal data — the standard
pattern that reconciles blockchains with GDPR.
"""
import threading
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass
from decimal import Decimal

from .core import (Blockchain, NodeWallet, Transaction, TransactionType, now_millis)


@dataclass(frozen=True)
class ContractResult:
    """Outcome of a smart-contract execution: the on-chain record plus a UI message."""
    contract_id: str
    accepted: bool
    message: str
    record: dict
    tx_id: str | None

    def with_tx_id(self, tx_id: str) -> "ContractResult":
        return ContractResult(self.contract_id, self.accepted, self.message,
                              self.record, tx_id)

    def to_dict(self) -> dict:
        return {
            "contractId": self.contract_id,
            "accepted": self.accepted,
            "message": self.message,
            "record": self.record,
            "txId": self.tx_id,
        }


class SmartContract(ABC):
    """Deterministic, auditable contract: same input → same result on every node."""

    @property
    @abstractmethod
    def contract_id(self) -> str:
        """Stable contract identifier recorded in the transaction payload."""

    @property
    @abstractmethod
    def description(self) -> str:
        """Sector / purpose description surfaced by the educational frontend."""

    @abstractmethod
    def execute(self, contract_input: dict) -> ContractResult:
        """
        Validates the input against the contract's rules and returns the state
        transition to be recorded on-chain. Raises ValueError on rule violations.
        """


def _require(contract_input: dict, key: str) -> str:
    value = contract_input.get(key)
    if value is None or not str(value).strip():
        raise ValueError(f"Missing contract input: {key}")
    return str(value)


class MedicalConsentContract(SmartContract):
    """
    Medical-sector contract: consent-based access to patient history. GDPR-compliant on
    an immutable ledger because no medical data and no real identity ever touches the
    chain — only pseudonymous identifiers (patientId/granteeId; in production, DID key
    fingerprints), a consent scope from a closed vocabulary, and a grant/revoke flag.
    The chain becomes a tamper-proof consent audit trail (GDPR art. 7 accountability):
    revocation is itself an immutable event — consent history can never be silently
    rewritten.
    """

    SCOPES = ("HISTORY_READ", "LAB_RESULTS", "IMAGING", "PRESCRIPTIONS", "FULL_RECORD")

    def __init__(self):
        self._consents: dict[str, list[dict]] = {}
        self._lock = threading.Lock()

    @property
    def contract_id(self) -> str:
        return "medical-consent-v1"

    @property
    def description(self) -> str:
        return ("Consent-based patient history access: pseudonymous, revocable, auditable "
                "consent records (GDPR-compatible: no personal/medical data on-chain).")

    def execute(self, contract_input: dict) -> ContractResult:
        patient_id = _require(contract_input, "patientId")
        grantee_id = _require(contract_input, "granteeId")
        scope = _require(contract_input, "scope")
        granted = _require(contract_input, "granted").lower() == "true"

        if scope not in self.SCOPES:
            raise ValueError(f"Unknown consent scope: {scope} (allowed: {list(self.SCOPES)})")

        record = {"patientId": patient_id, "granteeId": grantee_id, "scope": scope,
                  "granted": granted, "timestamp": now_millis()}
        with self._lock:
            self._consents.setdefault(patient_id, []).append(record)

        action = "granted to" if granted else "revoked from"
        return ContractResult(
            self.contract_id, True,
            f"Consent for scope {scope} {action} {grantee_id}",
            {"contract": self.contract_id, "patientId": patient_id,
             "granteeId": grantee_id, "scope": scope,
             "granted": "true" if granted else "false"},
            None)

    def history_of(self, patient_id: str) -> list[dict]:
        """Full consent history for a patient — the audit trail view."""
        with self._lock:
            return list(self._consents.get(patient_id, []))

    def has_consent(self, patient_id: str, grantee_id: str, scope: str) -> bool:
        """Access check used by hypothetical medical systems: latest record wins."""
        with self._lock:
            for record in reversed(self._consents.get(patient_id, [])):
                if record["granteeId"] == grantee_id and record["scope"] == scope:
                    return record["granted"]
        return False


class AgriSupplyChainContract(SmartContract):
    """
    Agriculture-sector contract: farm-to-fork supply chain transparency. Each batch
    accumulates an ordered, immutable trail of stage events, and stages only move
    FORWARD — a shipment cannot "return" to the farm on paper, which is precisely the
    class of document fraud this eliminates. Because every event is a signed ledger
    transaction, any consumer, importer or food-safety authority (e.g. under EU
    regulation 178/2002 traceability requirements) can audit the full provenance of a
    batch without trusting any single participant's database.
    """

    STAGES = ("HARVEST", "PROCESSING", "TRANSPORT", "WAREHOUSE", "RETAIL")

    def __init__(self):
        self._batches: dict[str, list[dict]] = {}
        self._lock = threading.Lock()

    @property
    def contract_id(self) -> str:
        return "agri-supply-chain-v1"

    @property
    def description(self) -> str:
        return ("Farm-to-fork traceability: forward-only, signed stage events per batch "
                "(harvest → processing → transport → warehouse → retail).")

    def execute(self, contract_input: dict) -> ContractResult:
        batch_id = _require(contract_input, "batchId")
        stage = _require(contract_input, "stage")
        actor = _require(contract_input, "actor")
        location = _require(contract_input, "location")
        details = str(contract_input.get("details") or "")

        if stage not in self.STAGES:
            raise ValueError(f"Unknown stage: {stage} (allowed: {list(self.STAGES)})")
        stage_index = self.STAGES.index(stage)

        with self._lock:
            trail = self._batches.setdefault(batch_id, [])
            if trail:
                last_index = self.STAGES.index(trail[-1]["stage"])
                if stage_index < last_index:
                    raise ValueError(f"Stage regression rejected: batch {batch_id} is "
                                     f"already at {trail[-1]['stage']}, cannot record {stage}")
            elif stage_index != 0:
                raise ValueError(f"Batch {batch_id} must start at HARVEST")
            trail.append({"batchId": batch_id, "stage": stage, "actor": actor,
                          "location": location, "details": details,
                          "timestamp": now_millis()})

        return ContractResult(
            self.contract_id, True,
            f"Batch {batch_id} recorded at stage {stage} ({location})",
            {"contract": self.contract_id, "batchId": batch_id, "stage": stage,
             "actor": actor, "location": location, "details": details},
            None)

    def trail_of(self, batch_id: str) -> list[dict]:
        """Full provenance trail of a batch — the consumer/auditor view."""
        with self._lock:
            return list(self._batches.get(batch_id, []))


class ContractEngine:
    """
    Executes a SmartContract and anchors the result on-chain: the contract's output
    record becomes the payload of a ledger transaction signed with this node's ML-DSA
    wallet. The chain therefore holds a signed, timestamped, tamper-evident proof of
    every contract execution — the "code is auditable law" property of Blockchain 3.0.
    """

    def __init__(self, blockchain: Blockchain, wallet: NodeWallet):
        self._blockchain = blockchain
        self._wallet = wallet

    def execute(self, contract: SmartContract, tx_type: TransactionType,
                contract_input: dict) -> ContractResult:
        result = contract.execute(contract_input)
        unsigned = Transaction(
            id=str(uuid.uuid4()), timestamp=now_millis(),
            sender=self._wallet.address, recipient=contract.contract_id,
            amount=Decimal(0), type=tx_type, payload=result.record,
            sender_public_key=self._wallet.public_key_b64, signature=None)
        from dataclasses import replace
        signed = replace(unsigned, signature=self._wallet.sign(unsigned.content_to_sign()))
        self._blockchain.submit(signed)
        return result.with_tx_id(signed.id)
