#pragma once

#include <cstdint>
#include <json/json.h>
#include <string>

namespace legalchain::nft {

/// A minted non-fungible token: on-chain certificate of digital ownership.
struct Nft {
    std::string tokenId;
    std::string title;
    std::string description;
    /// Pointer to off-chain metadata/asset (e.g. ipfs://...) — the ledger
    /// stores only this URI and its provenance, never the asset.
    std::string metadataUri;
    /// ML-DSA key fingerprint of the creator (self-sovereign identity).
    std::string creator;
    int64_t mintedAtMs;
    /// Ledger transaction that certifies the mint.
    std::string txId;

    Json::Value toJson() const {
        Json::Value v(Json::objectValue);
        v["tokenId"] = tokenId;
        v["title"] = title;
        v["description"] = description;
        v["metadataUri"] = metadataUri;
        v["creator"] = creator;
        v["mintedAt"] = static_cast<Json::Int64>(mintedAtMs);
        v["txId"] = txId;
        return v;
    }
};

} // namespace legalchain::nft
