"""
NFT minting: certifies digital ownership and creator identity (SSI) on the ledger.

How this certifies authorship without any registry or notary: the mint transaction is
signed with the creator's ML-DSA key, and the creator's address IS the fingerprint of
that key. Anyone can verify that whoever controls that key minted this token at this
time — self-sovereign identity in its purest form. The asset itself stays off-chain
behind metadataUri (IPFS in production: a content-addressed URI, so the metadata is
itself tamper-evident), keeping the chain lean and GDPR-safe.
"""
import threading
import uuid
from dataclasses import dataclass, replace
from decimal import Decimal

from .core import Blockchain, NodeWallet, Transaction, TransactionType, now_millis


@dataclass(frozen=True)
class Nft:
    """A minted non-fungible token: on-chain certificate of digital ownership."""
    token_id: str
    title: str
    description: str
    metadata_uri: str
    creator: str
    minted_at: int
    tx_id: str

    def to_dict(self) -> dict:
        return {
            "tokenId": self.token_id,
            "title": self.title,
            "description": self.description,
            "metadataUri": self.metadata_uri,
            "creator": self.creator,
            "mintedAt": self.minted_at,
            "txId": self.tx_id,
        }


class NftService:
    def __init__(self, blockchain: Blockchain, wallet: NodeWallet):
        self._blockchain = blockchain
        self._wallet = wallet
        self._minted: list[Nft] = []
        self._lock = threading.Lock()

    def mint(self, title: str, description: str, metadata_uri: str) -> Nft:
        token_id = str(uuid.uuid4())
        now = now_millis()
        unsigned = Transaction(
            id=str(uuid.uuid4()), timestamp=now,
            sender=self._wallet.address, recipient="nft-registry",
            amount=Decimal(0), type=TransactionType.NFT_MINT,
            payload={"tokenId": token_id, "title": title,
                     "description": description, "metadataUri": metadata_uri},
            sender_public_key=self._wallet.public_key_b64, signature=None)
        signed = replace(unsigned, signature=self._wallet.sign(unsigned.content_to_sign()))
        self._blockchain.submit(signed)

        nft = Nft(token_id, title, description, metadata_uri,
                  self._wallet.address, now, signed.id)
        with self._lock:
            self._minted.append(nft)
        return nft

    def all(self) -> list[Nft]:
        with self._lock:
            return list(self._minted)
