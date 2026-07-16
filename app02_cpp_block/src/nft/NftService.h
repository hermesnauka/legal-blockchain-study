#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "core/Blockchain.h"
#include "core/NodeWallet.h"
#include "nft/Nft.h"

namespace legalchain::nft {

/// NFT minting: certifies digital ownership and creator identity (SSI) on
/// the ledger.
///
/// How this certifies authorship without any registry or notary: the mint
/// transaction is signed with the creator's ML-DSA key, and the creator's
/// address *is* the fingerprint of that key. Anyone can verify that whoever
/// controls that key minted this token at this time — self-sovereign
/// identity in its purest form. The asset itself stays off-chain behind
/// metadataUri (IPFS in production: a content-addressed URI, so the
/// metadata is itself tamper-evident), keeping the chain lean and GDPR-safe.
class NftService {
public:
    NftService(core::Blockchain& blockchain, core::NodeWallet& wallet)
        : blockchain_(blockchain), wallet_(wallet) {}

    Nft mint(const std::string& title, const std::string& description, const std::string& metadataUri);
    std::vector<Nft> all() const;

private:
    core::Blockchain& blockchain_;
    core::NodeWallet& wallet_;
    mutable std::mutex mutex_;
    std::vector<Nft> minted_;
};

} // namespace legalchain::nft
