"""
The P2P ledger-synchronization protocol between two remote nodes, secured end-to-end
with post-quantum cryptography.

Handshake (QKD-inspired, MITM-resistant):
  1. HELLO (initiator → responder): the initiator's node id, its ML-DSA public key, a
     FRESH EPHEMERAL ML-KEM public key, and an ML-DSA signature over kemPublicKey|nodeId.
  2. ENCAPS (responder → initiator): the responder encapsulates a random 32-byte secret
     against the initiator's KEM key and returns the ciphertext, signed with its own
     ML-DSA key over ciphertext|nodeId.
  3. Both sides now hold the same one-time AES-256-GCM session key; every subsequent
     frame is a SECURE message (authenticated encryption).

Why a man-in-the-middle fails: the classic MITM move is substituting key material in
flight. Here both handshake messages are signed with ML-DSA, and each side checks that
the claimed node id equals the SHA3-256 fingerprint of the signing key. An attacker who
swaps the KEM key or the ciphertext must forge an ML-DSA signature — believed
infeasible even for a quantum adversary (lattice hardness, NIST FIPS 204). Tampering
with SECURE frames fails the GCM authentication tag instead.

Trust without revealing identity (ZKP-style pseudonymity): the handshake contains no
names, certificates or IP-bound identities. Each party proves exactly one statement —
"I control the private key behind this fingerprint" — by producing a valid signature,
and nothing else. Even the adopted ledger data is never trusted on channel security
alone: Blockchain.replace_chain re-audits every block, signature and consensus proof
locally before accepting a peer's chain.
"""
import json
import logging
import threading
import uuid

from .core import (Blockchain, Block, EventBus, LedgerEvent, LedgerEventType, NodeWallet)
from .crypto import PqcKeyExchangeService, PqcSignatureService, SecureChannel

log = logging.getLogger(__name__)

#: Chain frames carry several KB per signed transaction (ML-DSA sizes) — allow large messages.
MAX_MESSAGE_BYTES = 10 * 1024 * 1024


class PeerSession:
    """
    Mutable handshake state for one peer link. A link becomes `established` only after
    the ML-DSA-authenticated ML-KEM handshake has produced a one-time AES-256 session
    key; until then no ledger data flows. Transport-agnostic: `send` is any callable
    accepting the outgoing text frame (a WebSocket send, or a test double).
    """

    def __init__(self, send, initiator: bool, url: str | None = None):
        self.id = str(uuid.uuid4())
        self.send = send
        self.initiator = initiator
        self.url = url
        self.peer_node_id: str | None = None
        #: Initiator's ephemeral ML-KEM private key, discarded once the session key is derived.
        self.ephemeral_kem_private: bytes | None = None
        self.session_key: bytes | None = None

    def establish(self, session_key: bytes) -> None:
        self.session_key = session_key
        self.ephemeral_kem_private = None  # forward secrecy: ephemeral key is single-use

    @property
    def established(self) -> bool:
        return self.session_key is not None


class P2pService:
    """Protocol state machine; see the module docstring for the security narrative."""

    def __init__(self, blockchain: Blockchain, wallet: NodeWallet,
                 signatures: PqcSignatureService, key_exchange: PqcKeyExchangeService,
                 channel: SecureChannel, events: EventBus):
        self._blockchain = blockchain
        self._wallet = wallet
        self._signatures = signatures
        self._key_exchange = key_exchange
        self._channel = channel
        self._events = events
        self._peers: dict[str, PeerSession] = {}
        # Gossip: whenever this node seals a block, tell peers so they can pull the longer chain.
        events.subscribe(self._on_ledger_event)

    # ------------------------------------------------------------------ lifecycle

    def register(self, send, initiator: bool, url: str | None = None) -> PeerSession:
        peer = PeerSession(send, initiator, url)
        self._peers[peer.id] = peer
        if initiator:
            self._send_hello(peer)
        return peer

    def unregister(self, peer: PeerSession) -> None:
        self._peers.pop(peer.id, None)

    def connected_peers(self) -> list[dict]:
        return [{"nodeId": p.peer_node_id or "?", "url": p.url or "(inbound)"}
                for p in list(self._peers.values()) if p.established]

    # ------------------------------------------------------------------ handshake

    def _send_hello(self, peer: PeerSession) -> None:
        # Fresh ephemeral KEM key pair per connection: compromise of one session never
        # affects another (QKD-inspired key freshness).
        kem_public, kem_private = self._key_exchange.generate_keypair()
        peer.ephemeral_kem_private = kem_private
        kem_pub_b64 = self._key_exchange.encode_public_key(kem_public)
        self._send(peer, {
            "type": "HELLO",
            "nodeId": self._wallet.address,
            "dsaPub": self._wallet.public_key_b64,
            "kemPub": kem_pub_b64,
            # Signature binds the ephemeral KEM key to this node's ML-DSA identity.
            "signature": self._wallet.sign(kem_pub_b64 + "|" + self._wallet.address),
        })

    def on_message(self, peer: PeerSession, text: str) -> None:
        try:
            msg = json.loads(text)
            msg_type = msg.get("type")
            if msg_type == "HELLO":
                self._on_hello(peer, msg)
            elif msg_type == "ENCAPS":
                self._on_encaps(peer, msg)
            elif msg_type == "SECURE":
                self._on_secure(peer, msg)
            else:
                log.warning("Unknown P2P message type from %s", peer.id)
        except Exception as e:
            log.warning("Rejected P2P message (%s): %s", peer.id, e)

    def _on_hello(self, peer: PeerSession, msg: dict) -> None:
        node_id, dsa_pub = msg["nodeId"], msg["dsaPub"]
        kem_pub, signature = msg["kemPub"], msg["signature"]
        self._require_authentic(node_id, dsa_pub, kem_pub + "|" + node_id, signature)

        # Derive the one-time session key and send the signed ciphertext back.
        ciphertext_b64, session_key = self._key_exchange.encapsulate(kem_pub)
        peer.peer_node_id = node_id
        peer.establish(session_key)
        self._send(peer, {
            "type": "ENCAPS",
            "nodeId": self._wallet.address,
            "dsaPub": self._wallet.public_key_b64,
            "ciphertext": ciphertext_b64,
            "signature": self._wallet.sign(ciphertext_b64 + "|" + self._wallet.address),
        })
        self._events.publish(LedgerEvent(LedgerEventType.PEER_CONNECTED,
                                         {"nodeId": node_id, "direction": "inbound"}))
        log.info("PQC channel established with inbound peer %s", node_id)

    def _on_encaps(self, peer: PeerSession, msg: dict) -> None:
        node_id, dsa_pub = msg["nodeId"], msg["dsaPub"]
        ciphertext, signature = msg["ciphertext"], msg["signature"]
        self._require_authentic(node_id, dsa_pub, ciphertext + "|" + node_id, signature)

        if peer.ephemeral_kem_private is None:
            raise PermissionError("ENCAPS without a pending HELLO")
        peer.peer_node_id = node_id
        peer.establish(self._key_exchange.decapsulate(ciphertext, peer.ephemeral_kem_private))
        self._events.publish(LedgerEvent(LedgerEventType.PEER_CONNECTED,
                                         {"nodeId": node_id, "direction": "outbound"}))
        log.info("PQC channel established with outbound peer %s", node_id)
        # First act on a fresh secure channel: ask for the peer's ledger.
        self._send_secure(peer, {"type": "CHAIN_REQUEST"})

    def _require_authentic(self, node_id: str, dsa_pub: str, signed_content: str,
                           signature: str) -> None:
        """
        Verifies the handshake signature AND that the claimed node id is the fingerprint
        of the signing key — the identity binding that defeats key-substitution MITM.
        """
        if not self._signatures.verify(signed_content, signature, dsa_pub):
            raise PermissionError("Handshake rejected: invalid ML-DSA signature")
        try:
            fingerprint = self._signatures.fingerprint(
                self._signatures.decode_public_key(dsa_pub))
        except ValueError:
            raise PermissionError("Handshake rejected: malformed ML-DSA key")
        if fingerprint != node_id:
            raise PermissionError("Handshake rejected: node id is not the key fingerprint")

    # ------------------------------------------------------------------ secure traffic

    def _on_secure(self, peer: PeerSession, msg: dict) -> None:
        if not peer.established:
            raise PermissionError("SECURE frame before handshake completion")
        # GCM decryption raises on any tampering — integrity and confidentiality in one step.
        plaintext = self._channel.decrypt(msg["payload"], peer.session_key)
        inner = json.loads(plaintext)
        inner_type = inner.get("type")

        if inner_type == "CHAIN_REQUEST":
            self._send_secure(peer, {
                "type": "CHAIN_RESPONSE",
                "blocks": [b.to_dict() for b in self._blockchain.blocks()],
            })
        elif inner_type == "CHAIN_RESPONSE":
            remote_chain = [Block.from_dict(b) for b in inner["blocks"]]
            adopted = self._blockchain.replace_chain(remote_chain)
            log.info("Chain from %s: length=%d adopted=%s",
                     peer.peer_node_id, len(remote_chain), adopted)
        elif inner_type == "ANNOUNCE":
            if int(inner["length"]) > self._blockchain.length():
                self._send_secure(peer, {"type": "CHAIN_REQUEST"})
        else:
            log.warning("Unknown secure message from %s", peer.peer_node_id)

    def sync_with_peers(self) -> int:
        """Asks every established peer for its chain; longest valid chain wins locally."""
        asked = 0
        for peer in list(self._peers.values()):
            if peer.established:
                self._send_secure(peer, {"type": "CHAIN_REQUEST"})
                asked += 1
        return asked

    def _on_ledger_event(self, event: LedgerEvent) -> None:
        if event.type != LedgerEventType.BLOCK_ADDED:
            return
        for peer in list(self._peers.values()):
            if peer.established:
                self._send_secure(peer, {"type": "ANNOUNCE",
                                         "length": self._blockchain.length()})

    def _send_secure(self, peer: PeerSession, inner: dict) -> None:
        try:
            self._send(peer, {
                "type": "SECURE",
                "payload": self._channel.encrypt(json.dumps(inner), peer.session_key),
            })
        except Exception as e:
            log.warning("Failed to send secure frame to %s: %s", peer.peer_node_id, e)

    def _send(self, peer: PeerSession, message: dict) -> None:
        try:
            peer.send(json.dumps(message))
        except Exception as e:
            log.warning("Failed to send P2P frame: %s", e)


class EventBroadcaster:
    """
    Frontend live feed at /ws/events: relays every LedgerEvent to all connected
    browsers as {"type": ..., "data": ...} so the educational UI can animate blocks,
    transactions and peer connections in real time. This channel carries only data that
    is public on the ledger anyway, so unlike /ws/p2p it is not encrypted at the
    application layer.
    """

    def __init__(self, events: EventBus):
        self._sessions: dict[str, object] = {}
        events.subscribe(self._on_ledger_event)

    def attach(self, send) -> str:
        session_id = str(uuid.uuid4())
        self._sessions[session_id] = send
        return session_id

    def detach(self, session_id: str) -> None:
        self._sessions.pop(session_id, None)

    def _on_ledger_event(self, event: LedgerEvent) -> None:
        frame = json.dumps({"type": event.type.value, "data": event.data})
        for session_id, send in list(self._sessions.items()):
            try:
                send(frame)
            except Exception:
                log.debug("Dropping dead event session %s", session_id)
                self._sessions.pop(session_id, None)


def connect_to_peer(url: str, service: P2pService) -> None:
    """
    Outbound side of the P2P mesh: dials a remote peer's /ws/p2p endpoint with the
    thread-based `websockets` sync client and pumps incoming frames into the protocol
    state machine on a dedicated daemon thread — the Python analogue of the Java node's
    Loom virtual-thread client (cheap blocking I/O off the request path).
    """
    from websockets.sync.client import connect as ws_connect
    try:
        ws = ws_connect(url, max_size=MAX_MESSAGE_BYTES, open_timeout=10)
    except Exception as e:
        raise ValueError(f"Cannot connect to peer {url}: {e}")

    peer = service.register(ws.send, initiator=True, url=url)

    def receive_loop():
        try:
            for message in ws:
                service.on_message(peer, message)
        except Exception as e:
            log.debug("P2P link %s closed: %s", peer.id, e)
        finally:
            service.unregister(peer)

    threading.Thread(target=receive_loop, daemon=True,
                     name=f"p2p-client-{peer.id[:8]}").start()
