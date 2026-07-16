"""
The node's singleton object graph, hand-wired in dependency order — the Python
analogue of app01's Spring context, app02's AppContext and app03's DI registrations:
EventBus → crypto services → NodeWallet → ConsensusEngine → Tokenomics → Blockchain →
contracts → NFT → P2P. All chain state lives here, in memory: nothing can be edited
behind the ledger's back, and balances are always derived by replaying the chain.
"""
import threading
from decimal import Decimal

from django.conf import settings

from .consensus import (BftStrategy, ConsensusEngine, ProofOfStakeStrategy,
                        ProofOfWorkStrategy)
from .contracts import AgriSupplyChainContract, ContractEngine, MedicalConsentContract
from .core import Blockchain, EventBus, NodeWallet, TokenomicsService
from .crypto import PqcKeyExchangeService, PqcSignatureService, SecureChannel
from .nft import NftService
from .p2p import EventBroadcaster, P2pService


class AppContext:
    """One fully wired LegalChain node; tests may build as many as they need."""

    def __init__(self, node_name: str | None = None, consensus_default: str | None = None):
        conf = settings.LEGALCHAIN
        tokenomics_conf = conf["TOKENOMICS"]
        self.node_name = node_name or conf["NODE_NAME"]

        self.events = EventBus()
        self.signatures = PqcSignatureService()
        self.key_exchange = PqcKeyExchangeService()
        self.channel = SecureChannel()
        self.wallet = NodeWallet(self.signatures)
        self.consensus = ConsensusEngine(
            [ProofOfWorkStrategy(), ProofOfStakeStrategy(), BftStrategy()],
            consensus_default or conf["CONSENSUS_DEFAULT"])
        self.tokenomics = TokenomicsService(
            Decimal(tokenomics_conf["BLOCK_REWARD"]),
            int(tokenomics_conf["HALVING_INTERVAL"]),
            Decimal(tokenomics_conf["MAX_SUPPLY"]))
        self.blockchain = Blockchain(self.consensus, self.tokenomics,
                                     self.signatures, self.events)
        self.medical = MedicalConsentContract()
        self.agri = AgriSupplyChainContract()
        self.contract_engine = ContractEngine(self.blockchain, self.wallet)
        self.nft = NftService(self.blockchain, self.wallet)
        self.p2p = P2pService(self.blockchain, self.wallet, self.signatures,
                              self.key_exchange, self.channel, self.events)
        self.broadcaster = EventBroadcaster(self.events)


_context: AppContext | None = None
_context_lock = threading.Lock()


def get_context() -> AppContext:
    """Lazy process-wide singleton (ML-DSA key generation happens on first touch)."""
    global _context
    if _context is None:
        with _context_lock:
            if _context is None:
                _context = AppContext()
    return _context
