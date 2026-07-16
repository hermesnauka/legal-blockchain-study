"""
Post-quantum cryptography for the LegalChain ledger (Python/Django port).

Why this is compliant and secure: classical blockchain signatures (Bitcoin/Ethereum
ECDSA over secp256k1) rely on the discrete-logarithm problem, which Shor's algorithm
solves in polynomial time on a large fault-tolerant quantum computer. This node instead
uses the NIST-standardized lattice schemes — ML-DSA-65 (FIPS 204, from
CRYSTALS-Dilithium) for signatures and ML-KEM-768 (FIPS 203, from CRYSTALS-Kyber) for
key establishment — for which no quantum speed-up is known. Using NIST parameter sets
makes the ledger auditable against a concrete regulatory benchmark rather than ad-hoc
cryptography. The hash layer needs no swap: SHA3-256 keeps ~128-bit post-quantum
security because Grover's algorithm yields at most a quadratic speed-up.

Implementation note: `dilithium-py` and `kyber-py` are pure-Python reference
implementations of the final FIPS 204/203 standards — slow but ideal for teaching,
because every step is inspectable Python.
"""
import base64
import hashlib
import os

from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from dilithium_py.ml_dsa import ML_DSA_65
from kyber_py.ml_kem import ML_KEM_768


def sha3(data: str) -> str:
    """SHA3-256 of the UTF-8 bytes, as lowercase hex."""
    return hashlib.sha3_256(data.encode("utf-8")).hexdigest()


def merkle_root(leaves: list[str]) -> str:
    """
    Merkle root over the given leaf strings. The root commits to every transaction in a
    block: changing any transaction changes the root, which changes the block hash,
    which breaks every subsequent previousHash link — the tamper-evidence property that
    makes the ledger acceptable as a compliant general ledger.
    """
    if not leaves:
        return sha3("EMPTY")
    level = [sha3(leaf) for leaf in leaves]
    while len(level) > 1:
        nxt = []
        for i in range(0, len(level), 2):
            left = level[i]
            right = level[i + 1] if i + 1 < len(level) else left
            nxt.append(sha3(left + right))
        level = nxt
    return level[0]


class PqcSignatureService:
    """
    ML-DSA-65 signatures (FIPS 204). Every ledger transaction carries the sender's
    public key and signature, so any node — or any auditor — can verify authorship
    offline. Identity stays pseudonymous: the "address" is only a SHA3-256 fingerprint
    of the public key (self-sovereign identity), never a name.
    """

    ALGORITHM = "ML-DSA-65 (FIPS 204 / CRYSTALS-Dilithium)"

    def algorithm_name(self) -> str:
        return self.ALGORITHM

    def generate_keypair(self) -> tuple[bytes, bytes]:
        """Returns (public_key_bytes, private_key_bytes)."""
        return ML_DSA_65.keygen()

    def sign(self, content: str, private_key: bytes) -> str:
        """Signs UTF-8 content, returns Base64 signature."""
        return base64.b64encode(ML_DSA_65.sign(private_key, content.encode("utf-8"))).decode()

    def verify(self, content: str, signature_b64: str | None, public_key_b64: str | None) -> bool:
        """A malformed key or signature is simply an invalid signature, not a server error."""
        if not signature_b64 or not public_key_b64:
            return False
        try:
            return ML_DSA_65.verify(
                base64.b64decode(public_key_b64),
                content.encode("utf-8"),
                base64.b64decode(signature_b64),
            )
        except Exception:
            return False

    def decode_public_key(self, public_key_b64: str) -> bytes:
        try:
            raw = base64.b64decode(public_key_b64, validate=True)
        except Exception as e:
            raise ValueError("Malformed ML-DSA public key") from e
        if len(raw) != 1952:  # ML-DSA-65 public key size (FIPS 204)
            raise ValueError("Malformed ML-DSA public key")
        return raw

    def encode_public_key(self, public_key: bytes) -> str:
        return base64.b64encode(public_key).decode()

    def fingerprint(self, public_key: bytes) -> str:
        """
        Pseudonymous address: SHA3-256 fingerprint of the encoded public key. This is
        the ZKP-flavoured privacy property of the ledger — parties prove control of a
        key (by producing valid ML-DSA signatures) without revealing a real identity.
        """
        return "lc" + sha3(self.encode_public_key(public_key))[:40]


class PqcKeyExchangeService:
    """
    ML-KEM-768 key establishment (FIPS 203) — the QKD-inspired layer.

    True Quantum Key Distribution needs optical hardware, but its two security ideas
    are reproduced in software: (1) the shared symmetric key is freshly established per
    session and never stored or reused, and (2) its secrecy does not depend on any key a
    "harvest now, decrypt later" adversary could break retroactively — ML-KEM rests on
    the Module-LWE lattice problem, against which Shor's algorithm gives no advantage.
    MITM resistance comes from binding this exchange to ML-DSA identities in the P2P
    handshake (see ledger/p2p.py).
    """

    ALGORITHM = "ML-KEM-768 (FIPS 203 / CRYSTALS-Kyber)"

    def algorithm_name(self) -> str:
        return self.ALGORITHM

    def generate_keypair(self) -> tuple[bytes, bytes]:
        """Returns (encapsulation_key_bytes, decapsulation_key_bytes)."""
        return ML_KEM_768.keygen()

    def encapsulate(self, receiver_public_key_b64: str) -> tuple[str, bytes]:
        """Sender side: returns (ciphertext_b64 to transmit, 32-byte AES-256 session key)."""
        try:
            ek = base64.b64decode(receiver_public_key_b64)
            session_key, ciphertext = ML_KEM_768.encaps(ek)
        except Exception as e:
            raise ValueError("ML-KEM encapsulation failed (malformed key)") from e
        return base64.b64encode(ciphertext).decode(), session_key

    def decapsulate(self, ciphertext_b64: str, private_key: bytes) -> bytes:
        """Receiver side: recovers the same AES-256 session key from the ciphertext."""
        return ML_KEM_768.decaps(private_key, base64.b64decode(ciphertext_b64))

    def encode_public_key(self, public_key: bytes) -> str:
        return base64.b64encode(public_key).decode()


class SecureChannel:
    """
    Authenticated symmetric channel: AES-256-GCM under the ML-KEM-derived session key.

    GCM provides confidentiality and integrity in one primitive: any bit flipped in
    transit fails the authentication tag and the message is rejected — the QKD-style
    "disturbance detection". A fresh random 96-bit nonce is generated per message and
    prepended to the ciphertext; since session keys are one-time ML-KEM secrets, random
    nonces are safe at these message volumes. Wire format: Base64(nonce || ct || tag).
    """

    NONCE_BYTES = 12

    def encrypt(self, plaintext: str, session_key: bytes) -> str:
        nonce = os.urandom(self.NONCE_BYTES)
        sealed = AESGCM(session_key).encrypt(nonce, plaintext.encode("utf-8"), None)
        return base64.b64encode(nonce + sealed).decode()

    def decrypt(self, payload_b64: str, session_key: bytes) -> str:
        """Raises ValueError if the authentication tag is invalid (tampered or wrong key)."""
        payload = base64.b64decode(payload_b64)
        if len(payload) < self.NONCE_BYTES + 16:
            raise ValueError("AES-GCM payload too short (tampered)")
        try:
            plain = AESGCM(session_key).decrypt(
                payload[: self.NONCE_BYTES], payload[self.NONCE_BYTES:], None)
        except Exception as e:
            raise ValueError("AES-GCM decryption failed (tampered or wrong key)") from e
        return plain.decode("utf-8")
