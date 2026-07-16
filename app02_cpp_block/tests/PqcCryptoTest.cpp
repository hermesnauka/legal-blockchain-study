// Verifies the post-quantum primitives end to end: ML-DSA signatures (FIPS
// 204), ML-KEM key encapsulation (FIPS 203) and the AES-256-GCM secure
// channel built on it. Mirrors app01_java_block's PqcCryptoTest.java.
#include <gtest/gtest.h>

#include "crypto/PqcKeyExchangeService.h"
#include "crypto/PqcSignatureService.h"
#include "crypto/SecureChannel.h"

using namespace legalchain::crypto;

namespace {
PqcSignatureService signatures;
PqcKeyExchangeService keyExchange;
SecureChannel channel;
} // namespace

TEST(PqcCryptoTest, SignAndVerifyRoundTrip) {
    auto keyPair = signatures.generateKeyPair();
    std::string content = "compliant ledger entry";
    std::string signature = signatures.sign(content, keyPair.secretKey);

    EXPECT_TRUE(signatures.verify(content, signature, PqcSignatureService::encodePublicKey(keyPair.publicKey)));
}

TEST(PqcCryptoTest, TamperedContentFailsVerification) {
    auto keyPair = signatures.generateKeyPair();
    std::string signature = signatures.sign("original", keyPair.secretKey);

    EXPECT_FALSE(
        signatures.verify("tampered", signature, PqcSignatureService::encodePublicKey(keyPair.publicKey)));
}

TEST(PqcCryptoTest, SignatureFromAnotherKeyIsRejected) {
    auto alice = signatures.generateKeyPair();
    auto mallory = signatures.generateKeyPair();
    std::string signature = signatures.sign("payment", mallory.secretKey);

    EXPECT_FALSE(signatures.verify("payment", signature, PqcSignatureService::encodePublicKey(alice.publicKey)));
}

TEST(PqcCryptoTest, KemEncapsulationYieldsSameSecretOnBothSides) {
    auto receiver = keyExchange.generateKeyPair();

    Encapsulation sent = keyExchange.encapsulate(PqcKeyExchangeService::encodePublicKey(receiver.publicKey));
    auto received = keyExchange.decapsulate(sent.ciphertextBase64, receiver.secretKey);

    EXPECT_EQ(received, sent.sessionKey);
    EXPECT_EQ(received.size(), 32u); // AES-256
}

TEST(PqcCryptoTest, SecureChannelEncryptsAndAuthenticates) {
    auto receiver = keyExchange.generateKeyPair();
    Encapsulation enc = keyExchange.encapsulate(PqcKeyExchangeService::encodePublicKey(receiver.publicKey));

    std::string ciphertext = channel.encrypt("CHAIN_REQUEST", enc.sessionKey);
    EXPECT_EQ(channel.decrypt(ciphertext, enc.sessionKey), "CHAIN_REQUEST");

    // Any bit flip must fail the GCM authentication tag (MITM tamper detection).
    std::string tampered = ciphertext;
    tampered[10] = (tampered[10] == 'A') ? 'B' : 'A';
    EXPECT_THROW(channel.decrypt(tampered, enc.sessionKey), std::runtime_error);
}
