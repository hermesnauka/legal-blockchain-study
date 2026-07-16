"""Post-quantum primitives: ML-DSA signatures, ML-KEM key exchange, AES-GCM channel."""
import base64

from django.test import SimpleTestCase

from ledger.crypto import PqcKeyExchangeService, PqcSignatureService, SecureChannel


class PqcCryptoTest(SimpleTestCase):

    def test_ml_dsa_signature_round_trips_and_rejects_wrong_key_and_corruption(self):
        signatures = PqcSignatureService()
        public_key, private_key = signatures.generate_keypair()
        other_public_key, _ = signatures.generate_keypair()
        public_key_b64 = signatures.encode_public_key(public_key)

        content = "ledger-entry|42|lcabc|lcdef|10|TRANSFER|{}"
        signature = signatures.sign(content, private_key)

        self.assertTrue(signatures.verify(content, signature, public_key_b64))
        self.assertFalse(signatures.verify(content + "tampered", signature, public_key_b64))
        self.assertFalse(signatures.verify(
            content, signature, signatures.encode_public_key(other_public_key)))

        corrupted = bytearray(base64.b64decode(signature))
        corrupted[0] ^= 0xFF
        corrupted_b64 = base64.b64encode(bytes(corrupted)).decode()
        self.assertFalse(signatures.verify(content, corrupted_b64, public_key_b64))

    def test_fingerprint_is_stable_pseudonymous_address(self):
        signatures = PqcSignatureService()
        public_key, _ = signatures.generate_keypair()

        fingerprint = signatures.fingerprint(public_key)
        self.assertTrue(fingerprint.startswith("lc"))
        self.assertEqual(len(fingerprint), 42)
        self.assertEqual(fingerprint, signatures.fingerprint(public_key))

    def test_ml_kem_encapsulation_derives_the_same_session_key_on_both_sides(self):
        key_exchange = PqcKeyExchangeService()
        public_key, private_key = key_exchange.generate_keypair()
        public_key_b64 = key_exchange.encode_public_key(public_key)

        ciphertext_b64, sender_session_key = key_exchange.encapsulate(public_key_b64)
        receiver_session_key = key_exchange.decapsulate(ciphertext_b64, private_key)

        self.assertEqual(sender_session_key, receiver_session_key)
        self.assertEqual(len(sender_session_key), 32)

        other_ciphertext_b64, other_session_key = key_exchange.encapsulate(public_key_b64)
        self.assertNotEqual(sender_session_key, other_session_key)

    def test_secure_channel_encrypts_and_loudly_rejects_tampering(self):
        key_exchange = PqcKeyExchangeService()
        channel = SecureChannel()
        public_key, private_key = key_exchange.generate_keypair()
        public_key_b64 = key_exchange.encode_public_key(public_key)
        _, session_key = key_exchange.encapsulate(public_key_b64)

        plaintext = '{"type":"CHAIN_REQUEST"}'
        payload = channel.encrypt(plaintext, session_key)
        self.assertEqual(channel.decrypt(payload, session_key), plaintext)

        raw = bytearray(base64.b64decode(payload))
        raw[-1] ^= 0xFF
        tampered = base64.b64encode(bytes(raw)).decode()
        with self.assertRaises(ValueError):
            channel.decrypt(tampered, session_key)

        _, other_session_key = key_exchange.encapsulate(public_key_b64)
        with self.assertRaises(ValueError):
            channel.decrypt(payload, other_session_key)
