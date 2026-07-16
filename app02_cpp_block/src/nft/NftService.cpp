#include "nft/NftService.h"

#include "core/IdGen.h"

namespace legalchain::nft {

Nft NftService::mint(const std::string& title, const std::string& description, const std::string& metadataUri) {
    std::string tokenId = core::newUuid();
    int64_t now = core::nowMillis();

    core::Transaction unsigned_(
        core::newUuid(), now, wallet_.address(), "nft-registry", 0.0, core::TransactionType::NFT_MINT,
        std::map<std::string, std::string>{
            {"tokenId", tokenId}, {"title", title}, {"description", description}, {"metadataUri", metadataUri}},
        wallet_.publicKeyBase64(), std::nullopt);
    core::Transaction signed_(
        unsigned_.id(), unsigned_.timestampMs(), unsigned_.sender(), unsigned_.recipient(),
        unsigned_.amount(), unsigned_.type(), unsigned_.payload(), unsigned_.senderPublicKey(),
        wallet_.sign(unsigned_.contentToSign()));

    blockchain_.submit(signed_);

    Nft token{tokenId, title, description, metadataUri, wallet_.address(), now, signed_.id()};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        minted_.push_back(token);
    }
    return token;
}

std::vector<Nft> NftService::all() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return minted_;
}

} // namespace legalchain::nft
